#pragma once

namespace trajectory {

enum class GamepadPumpMode {
    preview,
    recording,
};

struct GamepadPumpResult {
    bool start_recording_requested{false};
    bool shutdown_requested{false};
};

}  // namespace trajectory
