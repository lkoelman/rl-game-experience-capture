#pragma once

#include <SDL3/SDL.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ViGEm/Common.h>

#include <array>
#include <cstdint>
#include <string>

namespace trajectory::virtual_gamepad {

// Cached physical controller state used by the bridge main loop.
// SDL delivers edge-triggered events, so the bridge keeps the latest observed
// axis and button values here and rebuilds a full XUSB report after each change.
struct PhysicalGamepadState {
    // Raw SDL axis values in the same slot order as SDL_GamepadAxis.
    std::array<short, SDL_GAMEPAD_AXIS_COUNT> axes{};
    // Pressed/not-pressed flags in the same slot order as SDL_GamepadButton.
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
};

// Applies one SDL axis event to the cached physical state.
void ApplyAxisMotion(PhysicalGamepadState& state, SDL_GamepadAxis axis, short value);

// Applies one SDL button press/release event to the cached physical state.
void ApplyButtonChange(PhysicalGamepadState& state, SDL_GamepadButton button, bool pressed);

// Converts the cached SDL-style state into the XUSB report expected by ViGEm.
XUSB_REPORT BuildXusbReport(const PhysicalGamepadState& state);

// Formats one forwarded button transition for bridge diagnostics.
std::string FormatForwardedButtonLogLine(SDL_JoystickID physical_gamepad_id,
                                         SDL_GamepadButton button,
                                         const std::string& virtual_gamepad_id);

class ForwardingLogRateLimiter {
public:
    explicit ForwardingLogRateLimiter(int max_lines_per_second);

    bool ShouldEmit(std::uint64_t now_monotonic_ns);

private:
    std::uint64_t min_interval_ns_{0};
    bool has_previous_emit_{false};
    std::uint64_t previous_emit_ns_{0};
};

}  // namespace trajectory::virtual_gamepad
