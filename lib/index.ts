// index.ts - Clean and simple exports
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// Type declaration for node-gyp-build
type NodeGypBuild = (dir: string) => any;

// Create require for CommonJS modules
const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));

// Load node-gyp-build and cast it
const nodeGypBuild = require('node-gyp-build') as NodeGypBuild;

import { Camera } from './camera.js'
import { CameraBuilder } from './builder.js'
import type { NativeAddon, SensorInfo } from './types.js'

const addon = nodeGypBuild(join(__dirname, '..')) as NativeAddon

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
