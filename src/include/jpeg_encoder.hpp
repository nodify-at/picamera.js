#pragma once

#include "common.hpp"
#include <turbojpeg.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace lcam {

/**
 * Asynchronous JPEG encoder using TurboJPEG
 */
class JpegEncoder {
public:
    explicit JpegEncoder(size_t maxQueueSize = 33);
    ~JpegEncoder();

    /**
     * Start encoder worker thread
     */
    void start();

    /**
     * Stop encoder and wait for thread to finish
     */
    void stop();

    /**
     * Queue YUV frame for JPEG encoding
     * @param yuvData YUV420 planar data
     * @param width Frame width
     * @param height Frame height
     * @param quality JPEG quality (1-100)
     * @param timestamp Frame timestamp
     * @param sequence Frame sequence number
     * @param callback Called when encoding complete
     */
    void encode(const uint8_t* yuvData, uint32_t width, uint32_t height,
                int quality, uint64_t timestamp, uint32_t sequence,
                FrameCallback callback);

private:
    struct Task {
        const uint8_t* data;
        uint32_t width;
        uint32_t height;
        int quality;
        uint64_t timestamp;
        uint32_t sequence;
        FrameCallback callback;
        std::shared_ptr<std::vector<uint8_t>> dataOwner;  // Keeps YUV data alive during encoding
    };

    void workerThread();

    tjhandle tjHandle_;               // TurboJPEG compressor instance
    std::thread worker_;              // Encoding thread
    std::queue<Task> queue_;          // Pending encode tasks
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};

    std::vector<uint8_t> buffer_;     // Reusable output buffer
    const size_t maxQueueSize_;       // Configurable max queue size
};

}
