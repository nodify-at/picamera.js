import type { AfMode, AwbMode, CameraConfig, Controls, ExposureMode, NativeAddon } from './types.js'
import { CameraError, ErrorCodes, validateDimensions, validateRange } from './types.js'
import { Camera } from './camera.js'

interface ImageQualityParams {
    brightness?: number
    contrast?: number
    saturation?: number
    sharpness?: number
}

/**
 * Fluent builder for camera configuration
 */
export class CameraBuilder {
    private config: CameraConfig = { streams: [] }

    constructor(private readonly addon: NativeAddon) {}

    /**
     * Configure RAW stream dimensions
     */
    raw(width = 2304, height = 1296): this {
        validateDimensions(width, height)
        this.config.rawStream = { width, height }
        return this
    }

    /**
     * Disable RAW stream to save memory
     */
    noRaw(): this {
        delete this.config.rawStream
        return this
    }

    /**
     * Add JPEG stream
     */
    jpeg(width = 1920, height = 1080): this {
        validateDimensions(width, height)
        this.config.streams.push({ type: 'jpeg', width, height })
        return this
    }

    /**
     * Add RGB stream
     */
    rgb(width = 640, height = 480): this {
        validateDimensions(width, height)
        this.config.streams.push({ type: 'rgb', width, height })
        return this
    }

    /**
     * Set JPEG encoder queue size
     */
    queueSize(size: number): this {
        validateRange(size, 1, 100, 'Queue size')
        this.config.jpegEncoderQueueSize = size
        return this
    }

    /**
     * Set target frame rate
     */
    fps(fps: number): this {
        validateRange(fps, 1, 120, 'FPS')
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.targetFps = fps
        }
        return this
    }

    /**
     * Set JPEG quality
     */
    quality(quality: number): this {
        validateRange(quality, 1, 100, 'JPEG quality')
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.jpegQuality = quality
        }
        return this
    }

    /**
     * Configure exposure
     */
    exposure(mode: ExposureMode, timeUs?: number): this {
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.exposureMode = mode
            if (timeUs !== undefined) {
                if (timeUs < 0) {
                    throw new CameraError('Exposure time cannot be negative', ErrorCodes.INVALID_EXPOSURE)
                }
                controls.exposureTime = timeUs
            }
        }
        return this
    }

    /**
     * Set gain (ISO equivalent)
     */
    gain(gain: number): this {
        if (gain < 0) {
            throw new CameraError('Gain cannot be negative', ErrorCodes.INVALID_GAIN)
        }
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.analogueGain = gain
        }
        return this
    }

    /**
     * Configure focus
     */
    focus(mode: AfMode, position?: number): this {
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.afMode = mode
            if (position !== undefined) {
                validateRange(position, 0.0, 1.0, 'Lens position')
                controls.lensPosition = position
            }
        }
        return this
    }

    /**
     * Configure white balance
     */
    whiteBalance(mode: AwbMode, redGain?: number, blueGain?: number): this {
        this.ensureControls()
        const controls = this.config.controls
        if (controls) {
            controls.awbMode = mode
            if (redGain !== undefined && blueGain !== undefined) {
                controls.colourGains = [redGain, blueGain]
            }
        }
        return this
    }

    /**
     * Adjust image quality
     */
    imageQuality(params: ImageQualityParams): this {
        this.ensureControls()
        const controls = this.config.controls
        if (!controls) return this

        if (params.brightness !== undefined) {
            validateRange(params.brightness, -1.0, 1.0, 'Brightness')
            controls.brightness = params.brightness
        }
        if (params.contrast !== undefined) {
            validateRange(params.contrast, -1.0, 1.0, 'Contrast')
            controls.contrast = params.contrast
        }
        if (params.saturation !== undefined) {
            validateRange(params.saturation, -1.0, 1.0, 'Saturation')
            controls.saturation = params.saturation
        }
        if (params.sharpness !== undefined) {
            validateRange(params.sharpness, -1.0, 1.0, 'Sharpness')
            controls.sharpness = params.sharpness
        }

        return this
    }

    /**
     * Set all controls at once
     */
    controls(controls: Controls): this {
        this.config.controls = { ...this.config.controls, ...controls }
        return this
    }

    /**
     * Build camera instance
     */
    build(): Camera {
        if (this.config.streams.length === 0) {
            throw new CameraError('At least one stream must be configured', ErrorCodes.NO_STREAMS)
        }
        return new Camera(this.addon, this.config)
    }

    /**
     * Build and start camera
     */
    buildAndStart(): Camera {
        const camera = this.build()
        if (!camera.start()) {
            throw new CameraError('Failed to start camera', ErrorCodes.START_FAILED)
        }
        return camera
    }

    private ensureControls(): void {
        if (!this.config.controls) {
            this.config.controls = {}
        }
    }
}
