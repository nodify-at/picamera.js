#include "stream_manager.hpp"
#include <sys/mman.h>
#include <cstring>
#include <iostream>

namespace lcam {
    StreamManager::StreamManager(std::shared_ptr<lc::Camera> camera)
        : camera_(camera), allocator_(std::make_unique<lc::FrameBufferAllocator>(camera)) {
    }

    StreamManager::~StreamManager() {
        freeBuffers();
    }

    bool StreamManager::configure(const std::optional<StreamConfig>& rawStream,
                                  const std::vector<StreamConfig> &configs) {
        std::vector<lc::StreamRole> roles;
        std::vector<StreamConfig> allConfigs;

        // Add RAW stream first if requested
        if (rawStream) {
            roles.push_back(lc::StreamRole::Raw);
            allConfigs.push_back(*rawStream);
        } else {
            // Default RAW stream for sensor configuration
            roles.push_back(lc::StreamRole::Raw);
            allConfigs.push_back({
                .type = StreamType::RAW,
                .width = 2304,  // Default sensor resolution
                .height = 1296
            });
        }

        // Add user-requested streams
        for (const auto &cfg: configs) {
            switch (cfg.type) {
                case StreamType::JPEG:
                    roles.push_back(lc::StreamRole::StillCapture);
                    break;
                case StreamType::RGB:
                    roles.push_back(lc::StreamRole::StillCapture);
                    break;
                case StreamType::RAW:
                    // Skip duplicate RAW
                    continue;
            }
            allConfigs.push_back(cfg);
        }

        config_ = camera_->generateConfiguration(roles);
        if (!config_) {
            std::cerr << "Failed to generate camera configuration" << std::endl;
            return false;
        }

        streamTypes_.clear();

        // Configure each stream
        for (size_t i = 0; i < allConfigs.size(); ++i) {
            auto &streamCfg = config_->at(i);
            const auto &appCfg = allConfigs[i];

            // Apply requested dimensions
            if (appCfg.width > 0) streamCfg.size.width = appCfg.width;
            if (appCfg.height > 0) streamCfg.size.height = appCfg.height;

            streamCfg.bufferCount = 6;  // Sufficient for smooth operation

            // Set pixel format based on stream type
            switch (appCfg.type) {
                case StreamType::RGB:
                    streamCfg.pixelFormat = lc::formats::BGR888;
                    break;
                case StreamType::JPEG:
                    streamCfg.pixelFormat = lc::formats::YUV420;
                    // Cache dimensions for encoder
                    jpegWidth_ = streamCfg.size.width;
                    jpegHeight_ = streamCfg.size.height;
                    break;
                case StreamType::RAW:
                    streamCfg.pixelFormat = lc::formats::SBGGR10;  // Bayer pattern
                    break;
            }
        }

        if (config_->validate() == lc::CameraConfiguration::Invalid) {
            std::cerr << "Invalid camera configuration after validation" << std::endl;
            return false;
        }

        if (camera_->configure(config_.get()) < 0) {
            std::cerr << "Failed to apply camera configuration" << std::endl;
            return false;
        }

        // Map configured streams to their types
        for (size_t i = 0; i < allConfigs.size(); ++i) {
            streamTypes_[config_->at(i).stream()] = allConfigs[i].type;
        }

        return true;
    }

    bool StreamManager::allocateBuffers() {
        // Allocate buffers for each stream
        for (auto &streamCfg: *config_) {
            auto stream = streamCfg.stream();

            if (allocator_->allocate(stream) < 0) {
                std::cerr << "Failed to allocate buffers for stream" << std::endl;
                return false;
            }

            const auto &buffers = allocator_->buffers(stream);
            std::vector<lc::FrameBuffer *> bufferPtrs;

            // Map each buffer for zero-copy access
            for (const auto &buffer: buffers) {
                auto *fb = buffer.get();
                bufferPtrs.push_back(fb);

                if (!mapBuffer(fb, streamTypes_[stream],
                               streamCfg.size.width, streamCfg.size.height)) {
                    std::cerr << "Failed to map buffer" << std::endl;
                    return false;
                }
            }

            streamBuffers_[stream] = bufferPtrs;
        }

        // Create capture requests
        const size_t numRequests = 6;  // Match buffer count
        for (size_t i = 0; i < numRequests; ++i) {
            auto request = camera_->createRequest();
            if (!request) {
                std::cerr << "Failed to create capture request" << std::endl;
                return false;
            }

            // Add buffers to request (round-robin)
            for (auto &streamCfg: *config_) {
                auto stream = streamCfg.stream();
                auto &buffers = streamBuffers_[stream];

                if (i < buffers.size()) {
                    request->addBuffer(stream, buffers[i]);
                }
            }

            requests_.push_back(std::move(request));
        }

        return true;
    }

    bool StreamManager::mapBuffer(lc::FrameBuffer *buffer, StreamType type,
                                  uint32_t width, uint32_t height) {
        // Skip mapping RAW buffers to save memory
        if (type == StreamType::RAW) return true;

        const auto &planes = buffer->planes();
        if (planes.empty()) return false;

        size_t totalSize = 0;

        switch (type) {
            case StreamType::JPEG:
                totalSize = width * height * 3 / 2; // YUV420
                break;
            case StreamType::RGB:
                totalSize = width * height * 3; // BGR888
                break;
            default:
                return true;
        }

        // Memory map for zero-copy access
        void *ptr = mmap(nullptr, totalSize, PROT_READ, MAP_SHARED,
                         planes[0].fd.get(), planes[0].offset);

        if (ptr == MAP_FAILED) {
            std::cerr << "Failed to mmap buffer: " << strerror(errno) << std::endl;
            return false;
        }

        mappedBuffers_[buffer] = {ptr, totalSize};
        return true;
    }

    void StreamManager::freeBuffers() {
        // Unmap all buffers
        for (auto &[buffer, mapped]: mappedBuffers_) {
            if (mapped.data) {
                munmap(mapped.data, mapped.size);
            }
        }
        mappedBuffers_.clear();

        // Free buffer allocations
        for (auto &[stream, _]: streamBuffers_) {
            allocator_->free(const_cast<lc::Stream *>(stream));
        }
        streamBuffers_.clear();

        requests_.clear();
    }

    void StreamManager::queueRequests() {
        for (auto &request: requests_) {
            camera_->queueRequest(request.get());
        }
    }

    StreamType StreamManager::getStreamType(const lc::Stream *stream) const {
        const auto it = streamTypes_.find(stream);
        return it != streamTypes_.end() ? it->second : StreamType::RAW;
    }

    lc::FrameBuffer *StreamManager::getBuffer(const lc::Stream *stream, size_t index) {
        auto it = streamBuffers_.find(stream);
        if (it != streamBuffers_.end() && index < it->second.size()) {
            return it->second[index];
        }
        return nullptr;
    }

    const uint8_t *StreamManager::getMappedData(lc::FrameBuffer *buffer) const {
        const auto it = mappedBuffers_.find(buffer);
        return it != mappedBuffers_.end() ? static_cast<const uint8_t *>(it->second.data) : nullptr;
    }

    size_t StreamManager::getMappedSize(lc::FrameBuffer *buffer) const {
        auto it = mappedBuffers_.find(buffer);
        return it != mappedBuffers_.end() ? it->second.size : 0;
    }
}
