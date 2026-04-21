#include "VirtualGamepadBridge.hpp"

#include <algorithm>
#include <cmath>

namespace trajectory::virtual_gamepad {

namespace {

unsigned char ScaleTrigger(short value) {
    const int clamped = std::clamp<int>(value, 0, 32767);
    return static_cast<unsigned char>(std::lround((static_cast<double>(clamped) * 255.0) / 32767.0));
}

void SetButtonIfPressed(const PhysicalGamepadState& state, SDL_GamepadButton button, XUSB_BUTTON mapped, XUSB_REPORT& report) {
    if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT && state.buttons[button]) {
        report.wButtons |= mapped;
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

    report.sThumbLX = state.axes[SDL_GAMEPAD_AXIS_LEFTX];
    report.sThumbLY = state.axes[SDL_GAMEPAD_AXIS_LEFTY];
    report.sThumbRX = state.axes[SDL_GAMEPAD_AXIS_RIGHTX];
    report.sThumbRY = state.axes[SDL_GAMEPAD_AXIS_RIGHTY];
    report.bLeftTrigger = ScaleTrigger(state.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]);
    report.bRightTrigger = ScaleTrigger(state.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]);

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

}  // namespace trajectory::virtual_gamepad
