import cv from '@u4/opencv4nodejs'
import { builder, Camera, FrameData } from '@nodify_at/picamera.js'

interface IEffect {
    readonly name: string

    apply(src: cv.Mat, frameNumber: number): cv.Mat
}

class OriginalEffect implements IEffect {
    readonly name = 'Original'

    apply(src: cv.Mat): cv.Mat {
        return src.copy()
    }
}

class EdgeDetectionEffect implements IEffect {
    readonly name = 'Edge Detection'

    apply(src: cv.Mat, frameNumber: number): cv.Mat {
        // grayscale → Canny → color-phase glow
        const gray = src.cvtColor(cv.COLOR_BGR2GRAY)
        const edges = gray.canny(50, 150)
        gray.release()

        // compute cycling RGB
        const phase = frameNumber * 0.05
        const [r, g, b] = [Math.sin(phase), Math.sin(phase + 2.094), Math.sin(phase + 4.189)].map(x =>
            Math.floor((x * 0.5 + 0.5) * 255),
        )

        // build 3-channel edge image
        const buf = edges.getData() // Uint8Array
        const colorBuf = Buffer.alloc(buf.length * 3)
        for (let i = 0; i < buf.length; i++) {
            const base = i * 3
            if (buf && buf[i]! > 0 && colorBuf[base] && b && g && r) {
                colorBuf[base] = b
                colorBuf[base + 1] = g
                colorBuf[base + 2] = r
            }
        }
        edges.release()

        const colored = new cv.Mat(colorBuf, src.rows, src.cols, cv.CV_8UC3)
        const blur = colored.gaussianBlur(new cv.Size(9, 9), 3)
        const dark = src.mul(0.3)
        const combined = dark.add(colored).add(blur.mul(0.5))

        colored.release()
        blur.release()
        dark.release()

        return combined
    }
}

class MatrixRainEffect implements IEffect {
    readonly name = 'Matrix Rain'

    apply(src: cv.Mat, frameNumber: number): cv.Mat {
        // Tint green
        const [b, g, r] = src.splitChannels()

        if (!b || !g || !r) throw new Error('Failed to split channels')

        const result = new cv.Mat([b.mul(0.1), g.mul(1.3), r.mul(0.1)]).mul(0.6)

        b.release()
        g.release()
        r.release()

        // draw falling bits
        const cols = 20
        const fontSize = 0.6
        for (let c = 0; c < cols; c++) {
            const x = 30 + c * 30
            const yBase = ((frameNumber * 2 + c * 20) % (src.rows + 40)) - 20
            for (let i = 0; i < 10; i++) {
                const y = (yBase + i * 30 + src.rows) % src.rows
                const brightness = 1 - (i / 10) * 0.7
                const ch = Math.random() < 0.5 ? '0' : '1'
                result.putText(
                    ch,
                    new cv.Point2(x, y),
                    cv.FONT_HERSHEY_SIMPLEX,
                    fontSize,
                    new cv.Vec3(0, Math.floor(255 * brightness), 0),
                    1,
                )
            }
        }

        return result
    }
}

class EffectsDemo {
    private camera: Camera
    private windowName = 'OpenCV Effects Demo'
    private effects: IEffect[] = [new OriginalEffect(), new EdgeDetectionEffect(), new MatrixRainEffect()]
    private idx = 0
    private frameNum = 0
    private effectFrames = 120 // ~4s @30fps
    private fpsCounter = 0
    private statsInterval?: NodeJS.Timeout

    constructor() {
        console.log('🎥 Starting demo… OpenCV', cv.version)
        this.camera = builder().rgb(640, 480).fps(30).build()
    }

    async start(): Promise<void> {
        cv.namedWindow(this.windowName, cv.WINDOW_NORMAL)
        cv.setWindowProperty(this.windowName, cv.WND_PROP_FULLSCREEN, cv.WINDOW_NORMAL | cv.WINDOW_OPENGL)
        this.camera.on('rgb', this.onFrame.bind(this))
        if (!this.camera.start()) {
            throw new Error('Failed to start camera')
        }
        console.log('✅ Camera started')
        this.statsInterval = setInterval(() => {
            console.log(`📊 FPS: ${(this.fpsCounter / 2) | 0}   🎨 ${this.effects[this.idx]?.name}`)
            this.fpsCounter = 0
        }, 2000)
    }

    private onFrame(frame: FrameData): void {
        this.fpsCounter++
        this.frameNum++
        if (this.frameNum % this.effectFrames === 0) {
            this.idx = (this.idx + 1) % this.effects.length
            console.log('Switched →', this.effects[this.idx]?.name)
        }

        const rgb = new cv.Mat(Buffer.from(frame.data), 480, 640, cv.CV_8UC3)
        const bgr = rgb.cvtColor(cv.COLOR_RGB2BGR)
        rgb.release()

        let out: cv.Mat | undefined
        try {
            out = this.effects[this.idx]?.apply(bgr, this.frameNum)
        } catch (err) {
            console.error('Effect error:', err)
            out = bgr.copy()
        }
        bgr.release()

        if (out) {
            this.drawLabel(out, this.effects[this.idx]?.name ?? '')
            cv.imshow(this.windowName, out)
            out.release()
        }

        cv.waitKey(1)
    }

    private drawLabel(mat: cv.Mat, text: string): void {
        const font = cv.FONT_HERSHEY_SIMPLEX
        const scale = 0.7,
            thickness = 2,
            pad = 10
        const { size: txtSize, baseLine } = cv.getTextSize(text, font, scale, thickness)

        mat.drawRectangle(
            new cv.Rect(pad, pad, txtSize.width + pad * 2, txtSize.height + pad * 2 + baseLine),
            new cv.Vec3(0, 0, 0),
            cv.FILLED,
        )
        mat.putText(
            text,
            new cv.Point2(pad * 2, pad * 2 + txtSize.height),
            font,
            scale,
            new cv.Vec3(255, 255, 255),
            thickness,
        )
    }

    stop(): void {
        console.log('🛑 Stopping…')
        if (this.statsInterval) clearInterval(this.statsInterval)
        this.camera.stop()
        cv.destroyAllWindows()
    }
}

async function main(): Promise<void> {
    const demo = new EffectsDemo()

    // shutdown hooks
    for (const sig of ['SIGINT', 'SIGTERM'] as const) {
        process.on(sig, () => {
            demo.stop()
            process.exit(0)
        })
    }

    try {
        await demo.start()
        // keep alive
        await new Promise(() => {})
    } catch (e) {
        console.error('Fatal:', e)
        demo.stop()
        process.exit(1)
    }
}

await main()
