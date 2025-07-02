#include "jpeg_encoder.hpp"
#include <algorithm>
#include <iostream>

namespace lcam {

JpegEncoder::JpegEncoder(size_t maxQueueSize)
    : tjHandle_(tjInitCompress()), maxQueueSize_(maxQueueSize) {
    if (!tjHandle_) {
        throw std::runtime_error("Failed to initialize TurboJPEG");
    }
    buffer_.reserve(2 * 1024 * 1024);  // 2MB initial size
}

JpegEncoder::~JpegEncoder() {
    stop();
    if (tjHandle_) {
        tjDestroy(tjHandle_);
    }
}

void JpegEncoder::start() {
    running_ = true;
    worker_ = std::thread(&JpegEncoder::workerThread, this);
}

void JpegEncoder::stop() {
    if (running_) {
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }
}

void JpegEncoder::encode(const uint8_t* yuvData, uint32_t width, uint32_t height,
                        int quality, uint64_t timestamp, uint32_t sequence,
                        FrameCallback callback) {
    // Copy YUV data to avoid it being overwritten during encoding
    size_t dataSize = width * height * 3 / 2;  // YUV420
    auto dataCopy = std::make_shared<std::vector<uint8_t>>(yuvData, yuvData + dataSize);

    {
        std::unique_lock<std::mutex> lock(mutex_);

        // Apply backpressure instead of dropping frames
        cv_.wait(lock, [this] {
            return queue_.size() < maxQueueSize_ || !running_;
        });

        if (!running_) return;

        // Warn if queue is getting full
        if (queue_.size() >= maxQueueSize_ - 2) {
            std::cerr << "JPEG encoder queue nearly full ("
                      << queue_.size() << "/" << maxQueueSize_ << ")" << std::endl;
        }

        queue_.push({
            dataCopy->data(),
            width,
            height,
            quality,
            timestamp,
            sequence,
            callback,
            dataCopy  // Keep data alive
        });
    }
    cv_.notify_one();
}

void JpegEncoder::workerThread() {
    while (running_) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            if (!running_) break;

            task = queue_.front();
            queue_.pop();
        }

        // Notify waiting encoders that queue has space
        cv_.notify_all();

        // Setup YUV plane pointers
        const uint8_t* planes[3];
        planes[0] = task.data;  // Y plane
        planes[1] = task.data + (task.width * task.height);  // U plane
        planes[2] = planes[1] + (task.width * task.height) / 4;  // V plane

        int strides[3] = {
            static_cast<int>(task.width),
            static_cast<int>(task.width / 2),
            static_cast<int>(task.width / 2)
        };

        // Ensure output buffer is large enough
        unsigned long maxSize = tjBufSizeYUV2(task.width, 1, task.height, TJSAMP_420);
        if (buffer_.size() < maxSize) {
            buffer_.resize(maxSize);
        }

        unsigned char* jpegBuf = buffer_.data();
        unsigned long jpegSize = maxSize;

        // Compress YUV to JPEG
        int result = tjCompressFromYUVPlanes(
            tjHandle_, planes, task.width, strides, task.height,
            TJSAMP_420, &jpegBuf, &jpegSize, task.quality,
            TJFLAG_FASTDCT | TJFLAG_NOREALLOC  // Fast DCT, no reallocation
        );

        if (result == 0) {
            // Create result buffer with exact size
            auto bufferCopy = std::make_shared<std::vector<uint8_t>>(
                buffer_.begin(), buffer_.begin() + jpegSize
            );

            Frame frame{
                std::span<const uint8_t>(bufferCopy->data(), bufferCopy->size()),
                task.timestamp,
                task.sequence,
                std::static_pointer_cast<void>(bufferCopy)
            };

            task.callback(StreamType::JPEG, frame);
        } else {
            std::cerr << "JPEG encoding failed: " << tjGetErrorStr() << std::endl;
        }
    }
}

}
