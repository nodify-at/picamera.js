#include "camera_manager.hpp"
#include "stream_manager.hpp"
#include "jpeg_encoder.hpp"
#include <iostream>
#include <algorithm>

namespace lcam {

class CameraManager::Impl {
public:
    Impl() : lcManager_(std::make_unique<lc::CameraManager>()) {}

    bool initialize(const CameraConfig& config) {
        if (lcManager_->start() < 0) {
            lastError_ = "Failed to start camera manager. Check if camera service is running.";
            return false;
        }

        auto cameras = lcManager_->cameras();
        if (cameras.empty()) {
            lastError_ = "No cameras found. Verify camera is connected and drivers are loaded.";
            return false;
        }

        // Use the first available camera
        camera_ = cameras[0];
        if (camera_->acquire()) {
            lastError_ = "Failed to acquire camera. Camera may be in use by another process.";
            return false;
        }

        streamManager_ = std::make_unique<StreamManager>(camera_);
        if (!streamManager_->configure(config.rawStream, config.streams)) {
            lastError_ = "Failed to configure streams. Check requested resolutions and formats.";
            return false;
        }

        controlManager_ = std::make_unique<ControlManager>(camera_);
        jpegEncoder_ = std::make_unique<JpegEncoder>(config.jpegEncoderQueueSize);

        initialControls_ = config.initialControls;

        return true;
    }

    bool start(const FrameCallback &frameCallback, ErrorCallback errorCallback) {
        frameCallback_ = frameCallback;
        errorCallback_ = errorCallback;

        if (!streamManager_->allocateBuffers()) {
            errorCallback_("Failed to allocate buffers. Insufficient memory or invalid configuration.");
            return false;
        }

        jpegEncoder_->start();

        // Connect to request completion signal
        camera_->requestCompleted.connect(this, &Impl::requestComplete);

        // Set default values if not specified
        lc::ControlList startControls;
        if (!initialControls_.targetFps) {
            initialControls_.targetFps = 30;
        }
        if (!initialControls_.jpegQuality) {
            initialControls_.jpegQuality = 85;
        }

        if (camera_->start(&startControls) < 0) {
            errorCallback_("Failed to start camera capture. Check camera permissions.");
            return false;
        }

        // Apply initial controls to all requests
        for (auto& request : streamManager_->requests()) {
            controlManager_->applyControls(initialControls_, request.get());
        }

        jpegQuality_ = initialControls_.jpegQuality.value_or(85);

        streamManager_->queueRequests();
        running_ = true;

        return true;
    }

    void stop() {
        if (!running_) return;

        running_ = false;
        camera_->requestCompleted.disconnect(this, &Impl::requestComplete);
        camera_->stop();

        jpegEncoder_->stop();
        streamManager_->freeBuffers();

        camera_->release();
        camera_.reset();
    }

    bool setControls(const Controls& controls) {
        std::lock_guard lock(controlMutex_);

        // JPEG quality is handled separately from camera controls
        if (controls.jpegQuality) {
            jpegQuality_ = *controls.jpegQuality;
        }

        pendingControls_ = controls;
        return true;
    }

    Controls getControls() const {
        return controlManager_->getCurrentControls();
    }

    ControlManager::Capabilities getCapabilities() const {
        return controlManager_->getCapabilities();
    }

private:
    /**
     * Called by libcamera when a capture request completes
     */
    void requestComplete(lc::Request* request) {
        if (request->status() == lc::Request::RequestCancelled) return;

        // Apply any pending control changes
        {
            std::lock_guard lock(controlMutex_);
            if (pendingControls_.has_value()) {
                controlManager_->applyControls(*pendingControls_, request);
                pendingControls_.reset();
            }
        }

        // Extract frame metadata
        uint32_t sequence = request->sequence();
        uint64_t timestamp = request->metadata().get(lc::controls::SensorTimestamp)
                                              .value_or(0);

        // Process each stream in the request
        for (auto& [stream, buffer] : request->buffers()) {
            auto type = streamManager_->getStreamType(stream);

            if (type == StreamType::RAW) continue;  // Skip RAW processing

            const uint8_t* data = streamManager_->getMappedData(buffer);
            size_t size = streamManager_->getMappedSize(buffer);

            if (!data) continue;

            if (type == StreamType::RGB) {
                // Direct delivery for RGB frames
                Frame frame{
                    std::span(data, size),
                    timestamp,
                    sequence,
                    nullptr
                };
                frameCallback_(StreamType::RGB, frame);
            } else if (type == StreamType::JPEG) {
                // Queue for async JPEG encoding
                jpegEncoder_->encode(
                    data,
                    streamManager_->getJpegWidth(),
                    streamManager_->getJpegHeight(),
                    jpegQuality_,
                    timestamp,
                    sequence,
                    frameCallback_
                );
            }
        }

        // Reuse request for next capture
        request->reuse(lc::Request::ReuseBuffers);
        camera_->queueRequest(request);
    }

    std::unique_ptr<lc::CameraManager> lcManager_;
    std::shared_ptr<lc::Camera> camera_;
    std::unique_ptr<StreamManager> streamManager_;
    std::unique_ptr<JpegEncoder> jpegEncoder_;
    std::unique_ptr<ControlManager> controlManager_;

    FrameCallback frameCallback_;
    ErrorCallback errorCallback_;

    Controls initialControls_;
    std::optional<Controls> pendingControls_;  // Controls waiting to be applied
    std::mutex controlMutex_;

    std::atomic<int> jpegQuality_{85};
    bool running_ = false;
    std::string lastError_;
};

// Public interface implementation
CameraManager::CameraManager() : pImpl(std::make_unique<Impl>()) {}
CameraManager::~CameraManager() = default;

bool CameraManager::initialize(const CameraConfig& config) const {
    return pImpl->initialize(config);
}

bool CameraManager::start(FrameCallback frameCallback, ErrorCallback errorCallback) const {
    return pImpl->start(frameCallback, errorCallback);
}

void CameraManager::stop() const {
    pImpl->stop();
}

bool CameraManager::setControls(const Controls& controls) {
    return pImpl->setControls(controls);
}

Controls CameraManager::getControls() const {
    return pImpl->getControls();
}

ControlManager::Capabilities CameraManager::getCapabilities() const {
    return pImpl->getCapabilities();
}

}
