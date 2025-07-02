// camera.ts - Simple camera wrapper without type conversions

import { EventEmitter } from 'node:events'
import type {
    CameraConfig,
    Controls,
    CameraEvent,
    FrameEvent,
    CameraCapabilities,
    Camera as NativeCamera,
    NativeAddon,
    FrameData,
    SensorInfo,
} from './types.js'
import { CameraError, isFrameEvent, isErrorEvent, ErrorCodes } from './types.js'

// Properly typed EventEmitter interface
export interface CameraEvents {
    jpeg: [frame: FrameData]
    rgb: [frame: FrameData]
    error: [error: CameraError]
    frame: [event: FrameEvent]
}

export declare interface Camera {
    on<K extends keyof CameraEvents>(event: K, listener: (...args: CameraEvents[K]) => void): this
    emit<K extends keyof CameraEvents>(event: K, ...args: CameraEvents[K]): boolean
    off<K extends keyof CameraEvents>(event: K, listener: (...args: CameraEvents[K]) => void): this
    removeAllListeners<K extends keyof CameraEvents>(event?: K): this
}

/**
 * Camera interface using libcamera
 */
export class Camera extends EventEmitter {
    private nativeCamera: NativeCamera
    private isRunning = false

    constructor(addon: NativeAddon, config: CameraConfig) {
        super()
        this.nativeCamera = new addon.Camera(config)
        this.setupEventHandler()
    }

    private setupEventHandler(): void {
        this.nativeCamera.on('frame', (event: CameraEvent) => {
            if (isErrorEvent(event)) {
                this.emit('error', new CameraError(event.error))
                return
            }

            if (isFrameEvent(event)) {
                // Emit specific stream events
                switch (event.stream) {
                    case 'jpeg':
                        this.emit('jpeg', event.frame)
                        break
                    case 'rgb':
                        this.emit('rgb', event.frame)
                        break
                }

                // Also emit a general frame event
                this.emit('frame', event)
            }
        })
    }

    /**
     * Start camera streaming
     */
    start(): boolean {
        if (this.isRunning) {
            throw new CameraError('Camera is already running', ErrorCodes.ALREADY_RUNNING)
        }

        const success = this.nativeCamera.start()
        if (success) {
            this.isRunning = true
        }
        return success
    }

    /**
     * Stop camera streaming
     */
    stop(): void {
        if (this.isRunning) {
            this.nativeCamera.stop()
            this.isRunning = false
            this.removeAllListeners()
        }
    }

    /**
     * Apply new control values
     */
    setControls(controls: Controls): boolean {
        return this.nativeCamera.setControls(controls)
    }

    /**
     * Get current control values
     */
    getControls(): Controls {
        return this.nativeCamera.getControls()
    }

    /**
     * Get camera hardware capabilities
     */
    getCapabilities(): CameraCapabilities {
        return this.nativeCamera.getCapabilities()
    }

    /**
     * Get sensor information
     */
    getSensorInfo(): SensorInfo {
        return this.nativeCamera.getSensorInfo()
    }

    /**
     * Check if camera is currently streaming
     */
    get running(): boolean {
        return this.isRunning
    }
}
