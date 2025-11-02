#pragma once

#include "common.hpp"
#include <mutex>
#include <map>
#include <vector>

namespace lcam {

/**
 * Manages camera streams, buffers, and memory mapping
 */
class StreamManager {
public:
    StreamManager(std::shared_ptr<lc::Camera> camera);
    ~StreamManager();

    /**
     * Configure streams based on requested types and resolutions
     */
    bool configure(const std::optional<StreamConfig>& rawStream,
                   const std::vector<StreamConfig>& configs);

    /**
     * Allocate frame buffers and create capture requests
     */
    bool allocateBuffers();

    /**
     * Free all allocated buffers and requests
     */
    void freeBuffers();

    /**
     * Queue all requests to start capture
     */
    void queueRequests();

    /**
     * Get stream type for a given libcamera stream
     */
    StreamType getStreamType(const lc::Stream* stream) const;

    /**
     * Get buffer at index for a stream
     */
    lc::FrameBuffer* getBuffer(const lc::Stream* stream, size_t index);

    /**
     * Get memory-mapped data pointer for a buffer
     */
    const uint8_t* getMappedData(lc::FrameBuffer* buffer) const;

    /**
     * Get size of mapped buffer data
     */
    size_t getMappedSize(lc::FrameBuffer* buffer) const;

    // JPEG stream dimensions for encoder
    uint32_t getJpegWidth() const { return jpegWidth_; }
    uint32_t getJpegHeight() const { return jpegHeight_; }

    const std::vector<std::unique_ptr<lc::Request>>& requests() const { return requests_; }

private:
    std::shared_ptr<lc::Camera> camera_;
    std::unique_ptr<lc::CameraConfiguration> config_;
    std::unique_ptr<lc::FrameBufferAllocator> allocator_;

    // Stream to type mapping
    std::map<const lc::Stream*, StreamType> streamTypes_;

    // Stream to buffer list mapping
    std::map<const lc::Stream*, std::vector<lc::FrameBuffer*>> streamBuffers_;

    // ARM64 cache-line aligned buffer structure
    struct alignas(64) MappedBuffer {
        void* data = nullptr;
        size_t size = 0;
    };

    // Buffer to mapped memory mapping
    std::map<lc::FrameBuffer*, MappedBuffer> mappedBuffers_;

    // Pre-allocated capture requests
    std::vector<std::unique_ptr<lc::Request>> requests_;

    // Cached JPEG dimensions
    uint32_t jpegWidth_ = 0;
    uint32_t jpegHeight_ = 0;

    /**
     * Memory-map a buffer for zero-copy access
     */
    bool mapBuffer(lc::FrameBuffer* buffer, StreamType type, uint32_t width, uint32_t height);
};

}
