#pragma once

#include "common.hpp"
#include <mutex>

namespace lcam {

/**
 * Manages camera controls and tracks their state
 */
class ControlManager {
public:
    ControlManager(std::shared_ptr<lc::Camera> camera);

    /**
     * Apply control changes to a capture request
     * @param controls New control values to apply
     * @param request Target request to modify
     */
    void applyControls(const Controls& controls, lc::Request* request);

    /**
     * Get snapshot of current control values
     */
    Controls getCurrentControls() const;

    struct Capabilities {
        struct Range {
            double min, max, def;
        };

        std::optional<Range> exposureTime;   // Microseconds
        std::optional<Range> analogueGain;    // Gain multiplier
        std::optional<Range> lensPosition;    // Focus distance
        std::vector<std::string> afModes;
        std::vector<std::string> awbModes;
    };

    /**
     * Query camera hardware capabilities
     */
    Capabilities getCapabilities() const;

private:
    std::shared_ptr<lc::Camera> camera_;
    mutable std::mutex mutex_;
    Controls currentControls_;  // Last applied values
    Controls pendingControls_;  // Merged pending changes
};

}
