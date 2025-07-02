#include <utility>
#include "control_manager.hpp"

namespace lcam {

ControlManager::ControlManager(std::shared_ptr<lc::Camera> camera)
    : camera_(std::move(camera)) {}

void ControlManager::applyControls(const Controls& controls, lc::Request* request) {
    std::lock_guard lock(mutex_);

    // Merge new controls with pending ones using helper template
    assignIfSet(pendingControls_.exposureMode, controls.exposureMode);
    assignIfSet(pendingControls_.exposureTime, controls.exposureTime);
    assignIfSet(pendingControls_.analogueGain, controls.analogueGain);
    assignIfSet(pendingControls_.afMode, controls.afMode);
    assignIfSet(pendingControls_.afTrigger, controls.afTrigger);
    assignIfSet(pendingControls_.lensPosition, controls.lensPosition);
    assignIfSet(pendingControls_.awbMode, controls.awbMode);
    assignIfSet(pendingControls_.colourGains, controls.colourGains);
    assignIfSet(pendingControls_.brightness, controls.brightness);
    assignIfSet(pendingControls_.contrast, controls.contrast);
    assignIfSet(pendingControls_.saturation, controls.saturation);
    assignIfSet(pendingControls_.sharpness, controls.sharpness);
    assignIfSet(pendingControls_.targetFps, controls.targetFps);
    assignIfSet(pendingControls_.jpegQuality, controls.jpegQuality);

    // Apply controls to the request
    auto& reqControls = request->controls();

    if (pendingControls_.exposureMode) {
        reqControls.set(lc::controls::AeExposureMode, *pendingControls_.exposureMode);
        currentControls_.exposureMode = pendingControls_.exposureMode;
    }

    if (pendingControls_.exposureTime) {
        reqControls.set(lc::controls::ExposureTime, *pendingControls_.exposureTime);
        currentControls_.exposureTime = pendingControls_.exposureTime;
    }

    if (pendingControls_.analogueGain) {
        reqControls.set(lc::controls::AnalogueGain, *pendingControls_.analogueGain);
        currentControls_.analogueGain = pendingControls_.analogueGain;
    }

    if (pendingControls_.afMode) {
        reqControls.set(lc::controls::AfMode, *pendingControls_.afMode);
        currentControls_.afMode = pendingControls_.afMode;
    }

    if (pendingControls_.afTrigger) {
        reqControls.set(lc::controls::AfTrigger, *pendingControls_.afTrigger);
        pendingControls_.afTrigger.reset();  // One-shot control
    }

    if (pendingControls_.lensPosition) {
        reqControls.set(lc::controls::LensPosition, *pendingControls_.lensPosition);
        currentControls_.lensPosition = pendingControls_.lensPosition;
    }

    if (pendingControls_.awbMode) {
        reqControls.set(lc::controls::AwbMode, *pendingControls_.awbMode);
        currentControls_.awbMode = pendingControls_.awbMode;
    }

    if (pendingControls_.colourGains && pendingControls_.colourGains->size() >= 2) {
        std::array<float, 2> gains = *pendingControls_.colourGains;
        reqControls.set(lc::controls::ColourGains, lc::Span<const float, 2>(gains.data(), 2));
        currentControls_.colourGains = pendingControls_.colourGains;
    }

    if (pendingControls_.brightness) {
        reqControls.set(lc::controls::Brightness, *pendingControls_.brightness);
        currentControls_.brightness = pendingControls_.brightness;
    }

    if (pendingControls_.contrast) {
        reqControls.set(lc::controls::Contrast, *pendingControls_.contrast);
        currentControls_.contrast = pendingControls_.contrast;
    }

    if (pendingControls_.saturation) {
        reqControls.set(lc::controls::Saturation, *pendingControls_.saturation);
        currentControls_.saturation = pendingControls_.saturation;
    }

    if (pendingControls_.sharpness) {
        reqControls.set(lc::controls::Sharpness, *pendingControls_.sharpness);
        currentControls_.sharpness = pendingControls_.sharpness;
    }

    if (pendingControls_.targetFps) {
        // Convert FPS to frame duration in microseconds
        const auto duration = 1000000 / *pendingControls_.targetFps;
        std::array<int64_t, 2> limits = {duration, duration};
        reqControls.set(lc::controls::FrameDurationLimits,
                       lc::Span<const int64_t, 2>(limits.data(), 2));
        currentControls_.targetFps = pendingControls_.targetFps;
    }

    // JPEG quality is handled by the encoder, not camera controls
    if (pendingControls_.jpegQuality) {
        currentControls_.jpegQuality = pendingControls_.jpegQuality;
    }
}

Controls ControlManager::getCurrentControls() const {
    std::lock_guard lock(mutex_);
    return currentControls_;
}

ControlManager::Capabilities ControlManager::getCapabilities() const {
    Capabilities caps;
    const auto& controls = camera_->controls();

    // Helper to extract min/max/default from control info
    auto extractRange = [&controls](const auto* control) -> std::optional<Capabilities::Range> {
        if (controls.count(control)) {
            const auto& info = controls.at(control);
            using T = typename std::decay_t<decltype(*control)>::type;
            return Capabilities::Range{
                static_cast<double>(info.min().template get<T>()),
                static_cast<double>(info.max().template get<T>()),
                static_cast<double>(info.def().template get<T>())
            };
        }
        return std::nullopt;
    };

    caps.exposureTime = extractRange(&lc::controls::ExposureTime);
    caps.analogueGain = extractRange(&lc::controls::AnalogueGain);
    caps.lensPosition = extractRange(&lc::controls::LensPosition);

    // Fixed capability lists
    caps.afModes = {"manual", "auto", "continuous"};
    caps.awbModes = {"auto", "incandescent", "tungsten", "fluorescent",
                     "indoor", "daylight", "cloudy", "custom"};

    return caps;
}

}
