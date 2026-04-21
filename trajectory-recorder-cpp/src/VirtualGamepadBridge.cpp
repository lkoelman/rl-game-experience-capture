#include "VirtualGamepadBridge.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace trajectory::virtual_gamepad {

namespace {

// SDL trigger axes are reported as signed 16-bit values, but XUSB expects
// unsigned trigger bytes. Negative values are treated as unpressed.
unsigned char ScaleTrigger(short value) {
    const int clamped = std::clamp<int>(value, 0, 32767);
    return static_cast<unsigned char>(std::lround((static_cast<double>(clamped) * 255.0) / 32767.0));
}

// Applies one boolean button slot from the cached SDL state to the XUSB mask.
void SetButtonIfPressed(const PhysicalGamepadState& state, SDL_GamepadButton button, XUSB_BUTTON mapped, XUSB_REPORT& report) {
    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT && state.buttons[button]) {
        report.wButtons |= mapped;
    }
}

const char* VirtualButtonName(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return "A";
    case SDL_GAMEPAD_BUTTON_EAST:
        return "B";
    case SDL_GAMEPAD_BUTTON_WEST:
        return "X";
    case SDL_GAMEPAD_BUTTON_NORTH:
        return "Y";
    case SDL_GAMEPAD_BUTTON_BACK:
        return "back";
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return "guide";
    case SDL_GAMEPAD_BUTTON_START:
        return "start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return "left_stick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return "right_stick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return "left_shoulder";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return "right_shoulder";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return "dpad_up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return "dpad_down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return "dpad_left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return "dpad_right";
    default:
        return "unknown_button";
    }
}

const char* PhysicalButtonName(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return "south";
    case SDL_GAMEPAD_BUTTON_EAST:
        return "east";
    case SDL_GAMEPAD_BUTTON_WEST:
        return "west";
    case SDL_GAMEPAD_BUTTON_NORTH:
        return "north";
    case SDL_GAMEPAD_BUTTON_BACK:
        return "back";
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return "guide";
    case SDL_GAMEPAD_BUTTON_START:
        return "start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return "left_stick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return "right_stick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return "left_shoulder";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return "right_shoulder";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return "dpad_up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return "dpad_down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return "dpad_left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return "dpad_right";
    default:
        return "unknown_button";
    }
}

}  // namespace

void ApplyAxisMotion(PhysicalGamepadState& state, SDL_GamepadAxis axis, short value) {
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) {
        return;
    }
    state.axes[axis] = value;
}

void ApplyButtonChange(PhysicalGamepadState& state, SDL_GamepadButton button, bool pressed) {
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT) {
        return;
    }
    state.buttons[button] = pressed;
}

XUSB_REPORT BuildXusbReport(const PhysicalGamepadState& state) {
    XUSB_REPORT report;
    XUSB_REPORT_INIT(&report);

    // Sticks map directly slot-for-slot from SDL's canonical gamepad layout.
    report.sThumbLX = state.axes[SDL_GAMEPAD_AXIS_LEFTX];
    report.sThumbLY = state.axes[SDL_GAMEPAD_AXIS_LEFTY];
    report.sThumbRX = state.axes[SDL_GAMEPAD_AXIS_RIGHTX];
    report.sThumbRY = state.axes[SDL_GAMEPAD_AXIS_RIGHTY];

    // Triggers are the only fields that need range conversion.
    report.bLeftTrigger = ScaleTrigger(state.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]);
    report.bRightTrigger = ScaleTrigger(state.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]);

    // The rest of the report is a button-mask projection from SDL's logical
    // button layout to XUSB's logical button layout.
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_SOUTH, XUSB_GAMEPAD_A, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_EAST, XUSB_GAMEPAD_B, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_WEST, XUSB_GAMEPAD_X, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_NORTH, XUSB_GAMEPAD_Y, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_BACK, XUSB_GAMEPAD_BACK, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_GUIDE, XUSB_GAMEPAD_GUIDE, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_START, XUSB_GAMEPAD_START, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_LEFT_STICK, XUSB_GAMEPAD_LEFT_THUMB, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_RIGHT_STICK, XUSB_GAMEPAD_RIGHT_THUMB, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XUSB_GAMEPAD_LEFT_SHOULDER, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XUSB_GAMEPAD_RIGHT_SHOULDER, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_DPAD_UP, XUSB_GAMEPAD_DPAD_UP, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_DPAD_DOWN, XUSB_GAMEPAD_DPAD_DOWN, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_DPAD_LEFT, XUSB_GAMEPAD_DPAD_LEFT, report);
    SetButtonIfPressed(state, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XUSB_GAMEPAD_DPAD_RIGHT, report);

    return report;
}

std::string FormatForwardedButtonLogLine(SDL_JoystickID physical_gamepad_id,
                                         SDL_GamepadButton button,
                                         const std::string& virtual_gamepad_id) {
    std::ostringstream formatted;
    formatted << physical_gamepad_id
              << '.'
              << PhysicalButtonName(button)
              << " -> "
              << virtual_gamepad_id
              << '.'
              << VirtualButtonName(button);
    return formatted.str();
}

ForwardingLogRateLimiter::ForwardingLogRateLimiter(int max_lines_per_second) {
    if (max_lines_per_second > 0) {
        min_interval_ns_ = static_cast<std::uint64_t>(1'000'000'000ULL / static_cast<std::uint64_t>(max_lines_per_second));
    }
}

bool ForwardingLogRateLimiter::ShouldEmit(std::uint64_t now_monotonic_ns) {
    if (!has_previous_emit_) {
        has_previous_emit_ = true;
        previous_emit_ns_ = now_monotonic_ns;
        return true;
    }

    if (min_interval_ns_ == 0 || now_monotonic_ns - previous_emit_ns_ >= min_interval_ns_) {
        previous_emit_ns_ = now_monotonic_ns;
        return true;
    }

    return false;
}

}  // namespace trajectory::virtual_gamepad
