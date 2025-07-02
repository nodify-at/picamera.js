// server.ts
import { WebSocket, WebSocketServer } from 'ws'
import { createServer, IncomingMessage } from 'http'
import { parse } from 'url'
import { AfMode, AwbMode, builder, Camera, ExposureMode, type FrameData } from './index.js'

interface ClientConnection {
    id: string
    ws: WebSocket
    lastFrameTime: number
    droppedFrames: number
    isAlive: boolean
    maxFps: number
    quality: 'high' | 'medium' | 'low'
}

interface ServerConfig {
    port: number
    maxClients: number
    cameraWidth: number
    cameraHeight: number
    cameraFps: number
    jpegQuality: number
}

interface FrameStats {
    totalFrames: number
    droppedFrames: number
    connectedClients: number
    avgFps: number
}

class WebSocketJpegStreamer {
    private readonly server: ReturnType<typeof createServer>
    private wss: WebSocketServer
    private camera: Camera | null = null
    private clients = new Map<string, ClientConnection>()
    private frameStats: FrameStats = { totalFrames: 0, droppedFrames: 0, connectedClients: 0, avgFps: 0 }
    private lastStatsTime = Date.now()
    private frameBuffer: Buffer | null = null

    constructor(private config: ServerConfig) {
        this.server = createServer(this.handleHttpRequest.bind(this))
        this.wss = new WebSocketServer({
            server: this.server,
            maxPayload: 5 * 1024 * 1024, // 5MB max payload
        })

        this.setupWebSocketHandlers()
        this.setupHealthCheck()
    }

    private setupWebSocketHandlers(): void {
        this.wss.on('connection', (ws: WebSocket, req: IncomingMessage) => {
            const clientId = this.generateClientId()
            const url = parse(req.url || '', true)

            const client: ClientConnection = {
                id: clientId,
                ws,
                lastFrameTime: 0,
                droppedFrames: 0,
                isAlive: true,
                maxFps: Math.min(Number(url.query['fps']) || 30, this.config.cameraFps),
                quality: (url.query['quality'] as 'high' | 'medium' | 'low') || 'high',
            }

            if (this.clients.size >= this.config.maxClients) {
                ws.close(1013, 'Server at capacity')
                return
            }

            this.clients.set(clientId, client)
            this.frameStats.connectedClients = this.clients.size

            console.log(`Client ${clientId} connected (${this.clients.size}/${this.config.maxClients})`)

            // Send initial metadata
            this.sendMessage(client, 'metadata', {
                width: this.config.cameraWidth,
                height: this.config.cameraHeight,
                fps: client.maxFps,
                quality: client.quality,
            })

            // Setup client handlers
            ws.on('message', (data: Buffer) => {
                try {
                    const message = JSON.parse(data.toString())
                    this.handleClientMessage(client, message)
                } catch (error) {
                    console.warn(`Invalid message from ${clientId}:`, error)
                }
            })

            ws.on('pong', () => {
                client.isAlive = true
            })

            ws.on('close', () => {
                this.clients.delete(clientId)
                this.frameStats.connectedClients = this.clients.size
                console.log(`Client ${clientId} disconnected (${this.clients.size} remaining)`)
            })

            ws.on('error', error => {
                console.error(`WebSocket error for ${clientId}:`, error)
                this.clients.delete(clientId)
                this.frameStats.connectedClients = this.clients.size
            })
        })

        this.wss.on('error', error => {
            console.error('WebSocket server error:', error)
        })
    }

    private handleClientMessage(client: ClientConnection, message: any): void {
        switch (message.type) {
            case 'setQuality':
                if (['high', 'medium', 'low'].includes(message.quality)) {
                    client.quality = message.quality
                    console.log(`Client ${client.id} changed quality to ${message.quality}`)
                }
                break
            case 'setFps':
                const fps = Math.min(Math.max(1, Number(message.fps) || 30), this.config.cameraFps)
                client.maxFps = fps
                console.log(`Client ${client.id} changed FPS to ${fps}`)
                break
        }
    }

    private setupHealthCheck(): void {
        // Ping clients every 30 seconds
        setInterval(() => {
            this.wss.clients.forEach(ws => {
                const client = Array.from(this.clients.values()).find(c => c.ws === ws)

                if (client) {
                    if (!client.isAlive) {
                        console.log(`Client ${client.id} failed ping, terminating`)
                        ws.terminate()
                        this.clients.delete(client.id)
                        return
                    }

                    client.isAlive = false
                    ws.ping()
                }
            })

            this.frameStats.connectedClients = this.clients.size
        }, 30000)

        // Log stats every 10 seconds
        setInterval(() => {
            this.logStats()
        }, 10000)
    }

    private handleHttpRequest(req: IncomingMessage, res: any): void {
        const parsedUrl = parse(req.url || '', true)

        switch (parsedUrl.pathname) {
            case '/stats':
                this.serveStats(res)
                break
            case '/health':
                res.writeHead(200, { 'Content-Type': 'application/json' })
                res.end(
                    JSON.stringify({
                        status: 'ok',
                        clients: this.clients.size,
                        camera: this.camera ? 'running' : 'stopped',
                    }),
                )
                break
            default:
                res.writeHead(404, { 'Content-Type': 'application/json' })
                res.end(JSON.stringify({ error: 'Not Found' }))
        }
    }

    private serveStats(res: any): void {
        res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' })
        res.end(
            JSON.stringify(
                {
                    ...this.frameStats,
                    cameraConfig: {
                        width: this.config.cameraWidth,
                        height: this.config.cameraHeight,
                        fps: this.config.cameraFps,
                        quality: this.config.jpegQuality,
                    },
                    uptime: process.uptime(),
                    memory: process.memoryUsage(),
                },
                null,
                2,
            ),
        )
    }

    private generateClientId(): string {
        return Date.now().toString(36) + Math.random().toString(36).substr(2)
    }

    private sendMessage(client: ClientConnection, type: string, data: any): void {
        if (client.ws.readyState === WebSocket.OPEN) {
            try {
                client.ws.send(JSON.stringify({ type, data }))
            } catch (error) {
                console.error(`Failed to send message to ${client.id}:`, error)
            }
        }
    }

    private broadcastFrame(frameData: FrameData): void {
        if (this.clients.size === 0) return

        this.frameStats.totalFrames++
        this.frameBuffer = frameData.data

        const now = Date.now()

        for (const client of this.clients.values()) {
            if (client.ws.readyState !== WebSocket.OPEN) continue

            try {
                // Send binary JPEG data directly
                client.ws.send(this.frameBuffer, { binary: true, compress: false })
                client.lastFrameTime = now
            } catch (error) {
                console.error(`Failed to send frame to ${client.id}:`, error)
                this.clients.delete(client.id)
            }
        }
    }

    private logStats(): void {
        const now = Date.now()
        const elapsed = (now - this.lastStatsTime) / 1000

        if (elapsed > 0) {
            this.frameStats.avgFps = Math.round((this.frameStats.totalFrames / elapsed) * 10) / 10
        }

        if (this.clients.size > 0) {
            console.log(
                `[Stats] Clients: ${this.frameStats.connectedClients}, ` +
                    `FPS: ${this.frameStats.avgFps}, ` +
                    `Frames: ${this.frameStats.totalFrames}, ` +
                    `Dropped: ${this.frameStats.droppedFrames}`,
            )
        }

        // Reset counters
        this.frameStats.totalFrames = 0
        this.frameStats.droppedFrames = 0
        this.lastStatsTime = now
    }

    public async start(): Promise<void> {
        try {
            // Initialize camera
            this.camera = builder()
                .jpeg(1280, 720)
                .rgb(640, 480)
                .raw()
                .fps(30)
                .quality(75)
                .exposure(ExposureMode.SHORT)
                .focus(AfMode.AUTO)
                .whiteBalance(AwbMode.AUTO)
                .build()

            // Setup frame handler
            this.camera.on('jpeg', frameData => {
                this.broadcastFrame(frameData)
            })

            this.camera.on('error', error => {
                console.error('Camera error:', error)
            })

            // Start camera
            if (!this.camera.start()) {
                throw new Error('Failed to start camera')
            }

            console.log(
                `Camera started: ${this.config.cameraWidth}x${this.config.cameraHeight} @ ${this.config.cameraFps}fps`,
            )

            // Start HTTP/WebSocket server
            return new Promise((resolve, reject) => {
                this.server.listen(this.config.port, () => {
                    console.log(`WebSocket server listening on port ${this.config.port}`)
                    console.log(`Stats endpoint: http://localhost:${this.config.port}/stats`)
                    console.log(`Health endpoint: http://localhost:${this.config.port}/health`)
                    resolve()
                })
                this.server.on('error', reject)
            })
        } catch (error) {
            console.error('Failed to start server:', error)
            await this.stop()
            throw error
        }
    }

    public async stop(): Promise<void> {
        console.log('Stopping server...')

        // Close all client connections
        for (const client of this.clients.values()) {
            client.ws.close(1001, 'Server shutting down')
        }
        this.clients.clear()

        // Close WebSocket server
        this.wss.close()

        // Stop camera
        if (this.camera) {
            this.camera.stop()
            this.camera = null
        }

        // Close HTTP server
        return new Promise(resolve => {
            this.server.close(() => {
                console.log('Server stopped')
                resolve()
            })
        })
    }

    public getStats(): FrameStats & { clients: number } {
        return { ...this.frameStats, clients: this.clients.size }
    }
}

// Server configuration
const config: ServerConfig = {
    port: parseInt(process.env['PORT'] || '8080'),
    maxClients: parseInt(process.env['MAX_CLIENTS'] || '10'),
    cameraWidth: parseInt(process.env['CAMERA_WIDTH'] || '1280'),
    cameraHeight: parseInt(process.env['CAMERA_HEIGHT'] || '720'),
    cameraFps: parseInt(process.env['CAMERA_FPS'] || '30'),
    jpegQuality: parseInt(process.env['JPEG_QUALITY'] || '85'),
}

// Create and start server
const server = new WebSocketJpegStreamer(config)

// Graceful shutdown handling
const shutdown = async (signal: string) => {
    console.log(`\nReceived ${signal}, shutting down gracefully...`)
    try {
        await server.stop()
        process.exit(0)
    } catch (error) {
        console.error('Error during shutdown:', error)
        process.exit(1)
    }
}

process.on('SIGINT', () => shutdown('SIGINT'))
process.on('SIGTERM', () => shutdown('SIGTERM'))
process.on('uncaughtException', error => {
    console.error('Uncaught exception:', error)
    shutdown('UNCAUGHT_EXCEPTION')
})

// Start server
server.start().catch(error => {
    console.error('Failed to start server:', error)
    process.exit(1)
})

export { WebSocketJpegStreamer, type ServerConfig }
