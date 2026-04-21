#pragma once

#include <SDL3/SDL.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ViGEm/Common.h>

#include <array>

namespace trajectory::virtual_gamepad {

struct PhysicalGamepadState {
    std::array<short, SDL_GAMEPAD_AXIS_COUNT> axes{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
};

void ApplyAxisMotion(PhysicalGamepadState& state, SDL_GamepadAxis axis, short value);
void ApplyButtonChange(PhysicalGamepadState& state, SDL_GamepadButton button, bool pressed);
XUSB_REPORT BuildXusbReport(const PhysicalGamepadState& state);

}  // namespace trajectory::virtual_gamepad
