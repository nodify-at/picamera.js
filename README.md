# 📷 @nodify/picamera.js

[![npm version](https://img.shields.io/npm/v/@nodify_at/picamera.js.svg)](https://www.npmjs.com/package/@nodify_at/picamera.js)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Node.js Version](https://img.shields.io/node/v/@nodify/picamera.js.svg)](https://nodejs.org)
[![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%205-ff69b4.svg)](https://www.raspberrypi.com/)

High-performance camera module for Raspberry Pi 5 using native libcamera bindings. Zero-copy memory architecture and hardware-accelerated JPEG encoding for efficient camera capture and streaming.

### 🎯 Why picamera.js?

**Dual-Channel Magic**: Capture 720p JPEG for recording while simultaneously processing 480p RGB for computer vision - all at just 20% CPU usage! This unique capability enables efficient architectures impossible with traditional single-stream solutions.

## Features

- **Zero-Copy Architecture** - Direct memory mapping eliminates buffer copies
- **Hardware JPEG Encoding** - TurboJPEG acceleration for fast compression
- **Dual-Channel Streams** - Simultaneous capture at different resolutions (e.g., 720p JPEG + 480p RGB)
- **Full Camera Control** - Exposure, focus, white balance, and image adjustments
- **TypeScript Support** - Complete type definitions included
- **Event-Driven** - Non-blocking async operation with EventEmitter
- **Fluent API** - Intuitive builder pattern for configuration
- **Low CPU Usage** - Only 20% CPU for 720p at 30fps

## Requirements

- Raspberry Pi 5 (ARM64)
- Node.js >= 20.0.0
- Raspberry Pi OS (64-bit)
- libcamera and development packages

## Installation

```bash
# Install system dependencies
sudo apt update
sudo apt install -y libcamera-dev libturbojpeg0-dev

# Install the package
npm install @nodify/picamera.js
```

## Quick Start

```javascript
import { createJpegCamera } from '@nodify/picamera.js';

// Create a simple JPEG camera
const camera = createJpegCamera(1920, 1080, 30);

// Handle frames
camera.on('jpeg', (frame) => {
    console.log(`Frame ${frame.sequence}: ${frame.data.length} bytes`);
    // frame.data is a Buffer containing JPEG data
});

// Handle errors
camera.on('error', (error) => {
    console.error('Camera error:', error);
});

// Start capturing
camera.start();

// Stop when needed
camera.stop();
```

## Dual-Channel Streaming

One of the most powerful features is simultaneous dual-channel capture at different resolutions. This enables efficient architectures where you can record/stream high quality while processing lower resolution:

```javascript
import { builder } from '@nodify/picamera.js';

const camera = builder()
    .jpeg(1280, 720)   // 720p JPEG for recording/streaming
    .rgb(640, 480)     // 480p RGB for real-time processing
    .fps(30)
    .quality(85)
    .build();

// High-quality JPEG stream for recording
camera.on('jpeg', (frame) => {
    // Save to file, stream to clients, etc.
    saveToFile(frame.data);
    streamToClients(frame.data);
});

// Lower resolution RGB for computer vision
camera.on('rgb', (frame) => {
    // Real-time processing without affecting recording
    detectObjects(frame.data, 640, 480);
});

camera.start();
```

This dual-channel approach uses only ~20% CPU while maintaining 30fps on both streams!

> **💡 Pro Tip**: Using dual channels (720p JPEG + 480p RGB) is more efficient than a single 1080p stream when you need both recording and processing. The hardware handles both streams in parallel with minimal overhead.

## API Reference

### Builder API

Create a camera using the fluent builder pattern:

```javascript
import { builder, controls } from '@nodify/picamera.js';

const camera = builder()
    // Configure streams
    .jpeg(1920, 1080)      // JPEG stream at 1920x1080
    .rgb(640, 480)         // RGB stream at 640x480
    .raw()                 // RAW sensor data (optional)
    
    // Set capture parameters
    .fps(30)               // Target frame rate
    .quality(85)           // JPEG quality (1-100)
    .queueSize(10)         // Frame buffer queue size
    
    // Build the camera
    .build();
```

### Camera Configuration

Configure camera controls during initialization:

```javascript
const camera = builder()
    .jpeg(1920, 1080)
    
    // Exposure controls
    .exposure(controls.ExposureMode.NORMAL, 10000)  // Mode and time in microseconds
    .gain(2.0)                                      // Analogue gain (ISO equivalent)
    
    // Focus controls
    .focus(controls.AfMode.CONTINUOUS)              // Autofocus mode
    
    // White balance
    .whiteBalance(controls.AwbMode.DAYLIGHT)        // AWB mode
    
    // Image quality adjustments (-1.0 to 1.0)
    .imageQuality({
        brightness: 0.0,
        contrast: 0.0,
        saturation: 0.0,
        sharpness: 0.0
    })
    
    .build();
```

### Camera Methods

#### `start(): boolean`
Starts camera capture. Returns `true` on success.

```javascript
if (camera.start()) {
    console.log('Camera started successfully');
}
```

#### `stop(): void`
Stops camera capture and releases resources.

```javascript
camera.stop();
```

#### `setControls(controls: Controls): boolean`
Updates camera controls while running.

```javascript
camera.setControls({
    exposureTime: 20000,    // 20ms exposure
    analogueGain: 4.0,      // Increase gain
    brightness: 0.5,        // Increase brightness
    contrast: 0.2
});
```

#### `getControls(): Controls`
Returns current control values.

```javascript
const controls = camera.getControls();
console.log('Current exposure time:', controls.exposureTime);
```

#### `getCapabilities(): CameraCapabilities`
Returns camera hardware capabilities and limits.

```javascript
const caps = camera.getCapabilities();
console.log('Exposure range:', caps.exposureTime);
// Output: { min: 14, max: 11767556, default: 1000 }
```

### Events

#### `jpeg` Event
Emitted when a JPEG frame is ready.

```javascript
camera.on('jpeg', (frame: FrameData) => {
    // frame.data: Buffer containing JPEG data
    // frame.timestamp: bigint nanosecond timestamp
    // frame.sequence: number frame sequence
});
```

#### `rgb` Event
Emitted when an RGB frame is ready.

```javascript
camera.on('rgb', (frame: FrameData) => {
    // frame.data: Buffer containing BGR888 pixel data
    // Width and height match configured stream size
});
```

#### `error` Event
Emitted when an error occurs.

```javascript
camera.on('error', (error: CameraError) => {
    console.error('Camera error:', error.message, error.code);
});
```

### Control Enums

```javascript
import { controls } from '@nodify/picamera.js';

// Exposure modes
controls.ExposureMode.NORMAL    // Standard auto exposure
controls.ExposureMode.SHORT     // Prefer shorter exposures
controls.ExposureMode.LONG      // Prefer longer exposures
controls.ExposureMode.CUSTOM    // Manual exposure control

// Autofocus modes
controls.AfMode.MANUAL          // Manual focus control
controls.AfMode.AUTO            // Single autofocus
controls.AfMode.CONTINUOUS      // Continuous autofocus

// White balance modes
controls.AwbMode.AUTO           // Automatic white balance
controls.AwbMode.INCANDESCENT  // Indoor incandescent lighting
controls.AwbMode.TUNGSTEN      // Tungsten lighting
controls.AwbMode.FLUORESCENT   // Fluorescent lighting
controls.AwbMode.INDOOR        // Generic indoor lighting
controls.AwbMode.DAYLIGHT      // Daylight
controls.AwbMode.CLOUDY        // Cloudy daylight
controls.AwbMode.CUSTOM        // Custom white balance
```

## Examples

### Simple JPEG Capture

```javascript
import { createJpegCamera } from '@nodify/picamera.js';
import fs from 'fs/promises';

const camera = createJpegCamera(1920, 1080, 30);

camera.on('jpeg', async (frame) => {
    // Save frame to file
    await fs.writeFile(`frame_${frame.sequence}.jpg`, frame.data);
});

camera.start();
```

### Multi-Stream Capture

```javascript
import { builder } from '@nodify/picamera.js';

const camera = builder()
    .jpeg(1920, 1080)  // High-resolution JPEG
    .rgb(640, 480)     // Lower resolution RGB for processing
    .fps(30)
    .build();

// Handle different streams
camera.on('jpeg', (frame) => {
    // Save or stream JPEG data
});

camera.on('rgb', (frame) => {
    // Process RGB data for computer vision
    const width = 640;
    const height = 480;
    const channels = 3; // BGR format
});

camera.start();
```

### Dynamic Control Adjustment

```javascript
import { builder, controls } from '@nodify/picamera.js';

const camera = builder().jpeg(1920, 1080).build();

// Start with auto exposure
camera.start();

// Switch to manual exposure after 5 seconds
setTimeout(() => {
    camera.setControls({
        exposureMode: controls.ExposureMode.CUSTOM,
        exposureTime: 10000,  // 10ms
        analogueGain: 2.0
    });
}, 5000);

// Trigger autofocus
camera.setControls({
    afTrigger: controls.AfTrigger.START
});
```

### WebSocket Streaming

```javascript
import { builder } from '@nodify/picamera.js';
import { WebSocketServer } from 'ws';

const camera = builder()
    .jpeg(1280, 720)
    .fps(30)
    .quality(80)
    .build();

const wss = new WebSocketServer({ port: 8080 });

camera.on('jpeg', (frame) => {
    // Broadcast to all connected clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(frame.data);
        }
    });
});

camera.start();
```

### Dual-Channel Use Cases

The dual-channel capability enables powerful architectures:

#### Security Camera with AI
```javascript
const camera = builder()
    .jpeg(1280, 720)   // HD recording
    .rgb(416, 416)     // YOLO input size
    .fps(20)
    .build();

// Record everything in HD
camera.on('jpeg', frame => {
    recordToNVR(frame);
});

// Run AI detection on optimized stream
camera.on('rgb', frame => {
    const detections = runYOLOv5(frame);
    if (detections.includes('person')) {
        sendAlert();
    }
});
```

#### Live Streaming with Local Display
```javascript
const camera = builder()
    .jpeg(1280, 720)   // For streaming
    .rgb(800, 480)     // For local LCD display
    .fps(30)
    .build();

// Stream to remote viewers
camera.on('jpeg', frame => {
    rtmpStream.write(frame.data);
});

// Display on local screen with overlays
camera.on('rgb', frame => {
    addTimestamp(frame);
    addWatermark(frame);
    displayOnLCD(frame);
});
```

## Performance

Typical resource usage on Raspberry Pi 5:

- **1280x720 @ 30 FPS (JPEG)**: ~20% CPU, ~35 MB RAM
- **720p JPEG + 480p RGB @ 30 FPS**: ~20% CPU, ~45 MB RAM
- **1920x1080 @ 30 FPS**: < 30% CPU, ~50 MB RAM
- **640x480 @ 60 FPS**: < 15% CPU, ~28 MB RAM

*Measured with JPEG encoding at 85% quality*

## Best Practices

### 🎯 Optimal Resolution Selection

```javascript
// ✅ GOOD: Use appropriate resolution for each task
const camera = builder()
    .jpeg(1280, 720)   // 720p is sufficient for most streaming
    .rgb(640, 480)     // Lower resolution for CV processing
    .build();

// ❌ AVOID: Over-provisioning resolution
const camera = builder()
    .jpeg(3840, 2160)  // 4K might be overkill for streaming
    .rgb(1920, 1080)   // Too high for real-time processing
    .build();
```

### 🔄 Dual-Channel Architecture

Leverage dual channels for efficient processing:

```javascript
// Recording + Analysis Pattern
const camera = builder()
    .jpeg(1280, 720)   // High quality for archive
    .rgb(320, 240)     // Ultra-low res for motion detection
    .fps(30)
    .build();

let recording = false;
let motionBuffer = [];

camera.on('rgb', (frame) => {
    // Continuous motion detection on low-res stream
    if (detectMotion(frame)) {
        recording = true;
        setTimeout(() => recording = false, 30000); // Record for 30s
    }
});

camera.on('jpeg', (frame) => {
    // Only save high-quality when motion detected
    if (recording) {
        motionBuffer.push(frame);
    }
});
```

### 📊 Resource Management

```javascript
// ✅ GOOD: Release resources properly
const camera = builder().jpeg(1280, 720).build();

process.on('SIGINT', () => {
    camera.stop();
    process.exit(0);
});

// ✅ GOOD: Handle backpressure
const frameQueue = [];
const MAX_QUEUE_SIZE = 30;

camera.on('jpeg', (frame) => {
    if (frameQueue.length < MAX_QUEUE_SIZE) {
        frameQueue.push(frame);
    } else {
        console.warn('Frame dropped - queue full');
    }
});

// ❌ AVOID: Unbounded queuing
camera.on('jpeg', async (frame) => {
    await slowDatabaseWrite(frame); // This will cause memory issues!
});
```

### 🎮 Dynamic Control Patterns

```javascript
// Adaptive Quality Based on Network
let networkQuality = 'good';

setInterval(() => {
    const quality = networkQuality === 'good' ? 85 : 60;
    camera.setControls({ jpegQuality: quality });
}, 5000);

// Scene-Based Adjustments
camera.on('rgb', (frame) => {
    const brightness = calculateAverageBrightness(frame);
    
    if (brightness < 50) {
        // Dark scene - increase exposure
        camera.setControls({
            exposureTime: 30000,
            analogueGain: 4.0
        });
    } else if (brightness > 200) {
        // Bright scene - decrease exposure
        camera.setControls({
            exposureTime: 5000,
            analogueGain: 1.0
        });
    }
});
```

### 🚀 Performance Optimization

```javascript
// ✅ GOOD: Process frames asynchronously
const processQueue = [];
const worker = new Worker('./frame-processor.js');

camera.on('jpeg', (frame) => {
    // Non-blocking handoff to worker
    worker.postMessage({ frame: frame.data });
});

// ✅ GOOD: Skip frames if needed
let frameCounter = 0;
camera.on('rgb', (frame) => {
    if (frameCounter++ % 3 === 0) {  // Process every 3rd frame
        analyzeFrame(frame);
    }
});

// ✅ GOOD: Use appropriate queue sizes
const camera = builder()
    .jpeg(1280, 720)
    .queueSize(5)  // Smaller queue for real-time apps
    .build();
```

### 🔧 Error Handling & Recovery

```javascript
class ResilientCamera {
    constructor() {
        this.restartAttempts = 0;
        this.maxRestarts = 5;
        this.initCamera();
    }

    initCamera() {
        this.camera = builder()
            .jpeg(1280, 720)
            .fps(30)
            .build();

        this.camera.on('error', (error) => {
            console.error('Camera error:', error);
            this.handleError(error);
        });

        this.camera.on('jpeg', this.onFrame.bind(this));
    }

    async handleError(error) {
        if (error.code === ErrorCodes.START_FAILED && 
            this.restartAttempts < this.maxRestarts) {
            console.log(`Attempting restart ${++this.restartAttempts}`);
            await this.restart();
        }
    }

    async restart() {
        try {
            this.camera.stop();
            await new Promise(resolve => setTimeout(resolve, 1000));
            if (this.camera.start()) {
                this.restartAttempts = 0;
                console.log('Camera restarted successfully');
            }
        } catch (err) {
            console.error('Restart failed:', err);
        }
    }

    onFrame(frame) {
        // Process frame
    }
}
```

### 💡 Common Patterns

#### Live Streaming with Adaptive Bitrate
```javascript
const camera = builder()
    .jpeg(1280, 720)
    .fps(30)
    .quality(80)
    .build();

// Monitor client bandwidth and adjust
function adjustQualityForClient(clientId, bandwidth) {
    if (bandwidth < 1000000) { // < 1 Mbps
        camera.setControls({ jpegQuality: 60 });
    } else if (bandwidth < 2000000) { // < 2 Mbps
        camera.setControls({ jpegQuality: 70 });
    } else {
        camera.setControls({ jpegQuality: 85 });
    }
}
```

#### Timelapse with Exposure Ramping
```javascript
const camera = builder()
    .jpeg(1920, 1080)
    .quality(95)
    .build();

let exposureTime = 10000; // Start at 10ms

async function captureTimelapse() {
    // Gradually increase exposure as it gets darker
    camera.setControls({ 
        exposureMode: controls.ExposureMode.CUSTOM,
        exposureTime: exposureTime 
    });
    
    camera.once('jpeg', async (frame) => {
        await saveFrame(frame);
        camera.stop();
        
        // Adjust exposure for next frame
        exposureTime = Math.min(exposureTime * 1.1, 100000);
        
        // Wait and capture next frame
        setTimeout(() => {
            camera.start();
            captureTimelapse();
        }, 60000); // 1 minute interval
    });
    
    camera.start();
}
```

## Error Handling

The library provides detailed error information through error codes:

```javascript
import { CameraError, ErrorCodes } from '@nodify/picamera.js';

camera.on('error', (error: CameraError) => {
    switch (error.code) {
        case ErrorCodes.ALREADY_RUNNING:
            console.log('Camera is already running');
            break;
        case ErrorCodes.START_FAILED:
            console.log('Failed to start camera');
            break;
        case ErrorCodes.INVALID_DIMENSION:
            console.log('Invalid resolution requested');
            break;
        // ... handle other error codes
    }
});
```

## TypeScript Support

Full TypeScript definitions are included:

```typescript
import { Camera, FrameData, Controls, CameraError } from '@nodify/picamera.js';

// All types are fully defined
const handleFrame = (frame: FrameData): void => {
    const data: Buffer = frame.data;
    const timestamp: bigint = frame.timestamp;
    const sequence: number = frame.sequence;
};

// Type-safe control configuration
const controls: Controls = {
    exposureTime: 10000,
    analogueGain: 2.0,
    jpegQuality: 90
};
```

## Troubleshooting

### Camera Not Found

```bash
# Verify camera is connected
libcamera-hello --list-cameras

# Enable camera interface
sudo raspi-config
# Navigate to: Interface Options -> Camera -> Enable
```

### Permission Issues

```bash
# Add user to video group
sudo usermod -a -G video $USER
# Log out and back in for changes to take effect
```

### High Memory Usage

- Reduce stream resolution
- Use only required streams (disable RAW if not needed)
- Adjust queue size based on your application needs

### Build Errors

```bash
# Ensure all dependencies are installed
sudo apt install -y libcamera-dev libturbojpeg0-dev build-essential

# Clear npm cache and rebuild
npm cache clean --force
npm rebuild
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Support

- 📧 Email: office@nodify.at
- 🐛 Issues: [GitHub Issues](https://github.com/nodify-at/picamera.js/issues)
- 💬 Discussions: [GitHub Discussions](https://github.com/nodify-at/picamera.js/discussions)

---

Built with ❤️ by [Nodify](https://nodify.at) for the Raspberry Pi community