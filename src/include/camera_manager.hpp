#pragma once

#include "common.hpp"
#include "control_manager.hpp"
#include <memory>

namespace lcam {

class StreamManager;
class JpegEncoder;

struct CameraConfig {
    std::optional<StreamConfig> rawStream;  // Optional RAW stream configuration
    std::vector<StreamConfig> streams;
    Controls initialControls;
    size_t jpegEncoderQueueSize = 33;  // Configurable JPEG encoder queue size
};

/**
 * Main camera interface using Pimpl pattern for ABI stability
 */
class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    /**
     * Initialize camera with given configuration
     * @return true on success, false on failure
     */
    bool initialize(const CameraConfig& config) const;

    /**
     * Start camera streaming
     * @param frameCallback Called for each captured frame
     * @param errorCallback Called on errors
     * @return true on success
     */
    bool start(FrameCallback frameCallback, ErrorCallback errorCallback) const;

    /**
     * Stop camera streaming
     */
    void stop() const;

    /**
     * Apply new control values (queued for next frame)
     */
    bool setControls(const Controls& controls);

    /**
     * Get current control values
     */
    Controls getControls() const;

    /**
     * Get camera hardware capabilities
     */
    ControlManager::Capabilities getCapabilities() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}
