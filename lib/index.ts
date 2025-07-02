// index.ts - Clean and simple exports

import bindings from 'bindings'
import { Camera } from './camera.js'
import { CameraBuilder } from './builder.js'
import type { NativeAddon, SensorInfo } from './types.js'

const addon = bindings('picamera.js') as NativeAddon

export { Camera, CameraBuilder }
export * from './types.js'

// Export control enums from native addon
export const controls = addon.controls

/**
 * Create camera builder
 */
export function builder(): CameraBuilder {
    return new CameraBuilder(addon)
}

/**
 * Create simple JPEG camera
 */
export function createJpegCamera(width = 1920, height = 1080, fps = 30): Camera {
    return builder()
        .jpeg(width, height)
        .fps(fps)
        .build()
}

/**
 * Get sensor information
 */
export function getSensorInfo(): SensorInfo {
    const tempCamera = new addon.Camera({ streams: [{ type: 'jpeg' }] })
    return tempCamera.getSensorInfo()
}
