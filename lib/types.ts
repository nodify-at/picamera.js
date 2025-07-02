// types.ts - Simple types that match C++ expectations directly

// Control enums
export const enum ExposureMode {
  NORMAL = 0,
  SHORT = 1,
  LONG = 2,
  CUSTOM = 3
}

export const enum AfMode {
  MANUAL = 0,
  AUTO = 1,
  CONTINUOUS = 2
}

export const enum AfTrigger {
  START = 0,
  CANCEL = 1
}

export const enum AwbMode {
  AUTO = 0,
  INCANDESCENT = 1,
  TUNGSTEN = 2,
  FLUORESCENT = 3,
  INDOOR = 4,
  DAYLIGHT = 5,
  CLOUDY = 6,
  CUSTOM = 7
}

// Core types - simple and direct
export interface StreamConfig {
  type: 'jpeg' | 'rgb' | 'raw'
  width?: number
  height?: number
}

export interface Controls {
  exposureMode?: ExposureMode
  exposureTime?: number
  analogueGain?: number
  afMode?: AfMode
  afTrigger?: AfTrigger
  lensPosition?: number
  awbMode?: AwbMode
  colourGains?: [number, number]  // [red, blue] - matches C++ expectation
  brightness?: number
  contrast?: number
  saturation?: number
  sharpness?: number
  targetFps?: number
  jpegQuality?: number
}

export interface CameraConfig {
  rawStream?: { width?: number; height?: number }
  streams: StreamConfig[]
  controls?: Controls
  jpegEncoderQueueSize?: number
}

// Frame data
export interface FrameData {
  data: Buffer
  timestamp: bigint
  sequence: number
}

export interface FrameEvent {
  type: 'frame'
  stream: 'jpeg' | 'rgb' | 'raw'
  frame: FrameData
}

export interface ErrorEvent {
  type: 'error'
  error: string
}

export type CameraEvent = FrameEvent | ErrorEvent

// Capabilities
export interface CapabilityRange {
  min: number
  max: number
  default: number
}

export interface CameraCapabilities {
  exposureTime?: CapabilityRange
  analogueGain?: CapabilityRange
  lensPosition?: CapabilityRange
  afModes: readonly string[]
  awbModes: readonly string[]
}

export interface SensorInfo {
  width: number
  height: number
  model: string
}

// Dimensions helper
export interface Dimensions {
  width: number
  height: number
}

// Native addon interface
export interface Camera {
  start(): boolean
  stop(): void
  setControls(controls: Controls): boolean
  getControls(): Controls
  getCapabilities(): CameraCapabilities
  getSensorInfo(): SensorInfo
  on(event: string, callback: (event: CameraEvent) => void): void
}

export interface CameraConstructor {
  new (config: CameraConfig): Camera
}

export interface NativeAddon {
  Camera: CameraConstructor
  controls: {
    ExposureMode: typeof ExposureMode
    AfMode: typeof AfMode
    AfTrigger: typeof AfTrigger
    AwbMode: typeof AwbMode
  }
}

// Error handling
export const ErrorCodes = {
  ALREADY_RUNNING: 'ALREADY_RUNNING',
  START_FAILED: 'START_FAILED',
  NOT_INITIALIZED: 'NOT_INITIALIZED',
  INVALID_DIMENSION: 'INVALID_DIMENSION',
  OUT_OF_RANGE: 'OUT_OF_RANGE',
  INVALID_EXPOSURE: 'INVALID_EXPOSURE',
  INVALID_GAIN: 'INVALID_GAIN',
  NO_STREAMS: 'NO_STREAMS',
} as const

export type ErrorCode = typeof ErrorCodes[keyof typeof ErrorCodes]

export class CameraError extends Error {
  constructor(message: string, public readonly code?: ErrorCode) {
    super(message)
    this.name = 'CameraError'
  }
}

// Type guards
export function isFrameEvent(event: CameraEvent): event is FrameEvent {
  return event.type === 'frame'
}

export function isErrorEvent(event: CameraEvent): event is ErrorEvent {
  return event.type === 'error'
}

// Validation helpers
export function validateDimensions(width: number, height: number): void {
  if (width <= 0 || width > 8192) {
    throw new CameraError(`Invalid width: ${width} (must be 1-8192)`, ErrorCodes.INVALID_DIMENSION)
  }
  if (height <= 0 || height > 8192) {
    throw new CameraError(`Invalid height: ${height} (must be 1-8192)`, ErrorCodes.INVALID_DIMENSION)
  }
}

export function validateRange(value: number, min: number, max: number, name: string): void {
  if (value < min || value > max) {
    throw new CameraError(`${name} out of range: ${value} (must be ${min}-${max})`, ErrorCodes.OUT_OF_RANGE)
  }
}
