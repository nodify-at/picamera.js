#pragma once

#include <libcamera/libcamera.h>
#include <memory>
#include <span>
#include <optional>
#include <functional>
#include <string>
#include <chrono>
#include <vector>
#include <array>

namespace lc = libcamera;
namespace lcam {

enum class StreamType {
    JPEG,
    RGB,
    RAW
};

struct StreamConfig {
    StreamType type;
    uint32_t width = 0;  // 0 means use camera default
    uint32_t height = 0; // 0 means use camera default
};

struct Frame {
    std::span<const uint8_t> data;
    uint64_t timestamp;    // Nanoseconds since epoch
    uint32_t sequence;     // Frame sequence number
    std::shared_ptr<void> owner;  // Keeps underlying buffer alive
};

struct Controls {
    // Exposure controls
    std::optional<int32_t> exposureMode;    // 0=Normal, 1=Short, 2=Long, 3=Custom
    std::optional<int32_t> exposureTime;    // Microseconds
    std::optional<float> analogueGain;      // Sensor gain multiplier

    // Focus controls
    std::optional<int32_t> afMode;          // 0=Manual, 1=Auto, 2=Continuous
    std::optional<int32_t> afTrigger;       // 0=Start, 1=Cancel (one-shot)
    std::optional<float> lensPosition;      // 0.0=infinity, 1.0=macro

    // White balance
    std::optional<int32_t> awbMode;         // AWB algorithm selection
    std::optional<std::array<float, 2>> colourGains; // [red, blue] gains

    // Image quality adjustments (-1.0 to 1.0)
    std::optional<float> brightness;
    std::optional<float> contrast;
    std::optional<float> saturation;
    std::optional<float> sharpness;

    // Performance
    std::optional<int32_t> targetFps;       // Target frame rate
    std::optional<int32_t> jpegQuality;     // 1-100, higher is better
};

// Helper template to reduce boilerplate
template<typename T>
inline void assignIfSet(std::optional<T>& target, const std::optional<T>& source) {
    if (source) target = source;
}

using FrameCallback = std::function<void(StreamType type, const Frame& frame)>;
using ErrorCallback = std::function<void(const std::string& error)>;

}
