#include "GamepadBindingCapture.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <SDL3/SDL.h>

namespace trajectory::mapping {

namespace {

// Normalizes SDL axis values into the mapper's expected [-1, 1] range.
float NormalizeAxisValue(Sint16 value) {
    const float normalized = static_cast<float>(value) / 32767.0f;
    return std::clamp(normalized, -1.0f, 1.0f);
}

// Converts SDL button enums into the stable YAML control names used by profiles.
std::string ButtonControlName(SDL_GamepadButton button) {
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
    case SDL_GAMEPAD_BUTTON_MISC1:
        return "misc1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
        return "right_paddle1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
        return "left_paddle1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
        return "right_paddle2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
        return "left_paddle2";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
        return "touchpad";
    default:
        return {};
    }
}

// Converts SDL axis enums into the stable YAML control names used by profiles.
std::string AxisControlName(SDL_GamepadAxis axis) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        return "leftx";
    case SDL_GAMEPAD_AXIS_LEFTY:
        return "lefty";
    case SDL_GAMEPAD_AXIS_RIGHTX:
        return "rightx";
    case SDL_GAMEPAD_AXIS_RIGHTY:
        return "righty";
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        return "left_trigger";
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        return "right_trigger";
    default:
        return {};
    }
}

// Identifies joystick axes that should map to analog action bindings.
bool IsAnalogAxis(SDL_GamepadAxis axis) {
    return axis == SDL_GAMEPAD_AXIS_LEFTX || axis == SDL_GAMEPAD_AXIS_LEFTY ||
           axis == SDL_GAMEPAD_AXIS_RIGHTX || axis == SDL_GAMEPAD_AXIS_RIGHTY;
}

// Identifies trigger axes that should map to threshold-based trigger bindings.
bool IsTriggerAxis(SDL_GamepadAxis axis) {
    return axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

}  // namespace

GamepadBindingCapture::GamepadBindingCapture() = default;

GamepadBindingCapture::~GamepadBindingCapture() {
    Stop();
}

void GamepadBindingCapture::Start() {
    if (started_) {
        return;
    }

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        throw std::runtime_error(SDL_GetError());
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr && count > 0) {
        gamepad_ = SDL_OpenGamepad(ids[0]);
    }
    SDL_free(ids);

    started_ = true;
}

void GamepadBindingCapture::Stop() {
    if (!started_) {
        return;
    }
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    started_ = false;
}

std::optional<ObservedBinding> GamepadBindingCapture::WaitForBinding(ActionInputKind kind,
                                                                     std::chrono::milliseconds timeout,
                                                                     std::string& error) {
    if (!started_) {
        error = "gamepad capture has not been started";
        return std::nullopt;
    }

    const Uint64 start = SDL_GetTicks();
    while ((SDL_GetTicks() - start) < static_cast<Uint64>(timeout.count())) {
        SDL_Event event;
        bool saw_event = false;
        while (SDL_PollEvent(&event)) {
            saw_event = true;
            switch (event.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                if (gamepad_ == nullptr) {
                    gamepad_ = SDL_OpenGamepad(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepad_ != nullptr && SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad_);
                    gamepad_ = nullptr;
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (kind == ActionInputKind::digital) {
                    const std::string control = ButtonControlName(static_cast<SDL_GamepadButton>(event.gbutton.button));
                    if (!control.empty()) {
                        return ObservedBinding{ActionBinding::Button(control), "button " + control};
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                const SDL_GamepadAxis axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
                const float value = NormalizeAxisValue(event.gaxis.value);
                if (kind == ActionInputKind::analog && IsAnalogAxis(axis) && std::fabs(value) >= 0.35f) {
                    const std::string control = AxisControlName(axis);
                    return ObservedBinding{ActionBinding::Axis(control, "any"), "axis " + control};
                }
                if (kind == ActionInputKind::trigger && IsTriggerAxis(axis) && value >= 0.2f) {
                    const std::string control = AxisControlName(axis);
                    const float threshold = std::clamp(value * 0.8f, 0.2f, 1.0f);
                    return ObservedBinding{ActionBinding::Trigger(control, threshold), "trigger " + control};
                }
                break;
            }
            default:
                break;
            }
        }

        if (!saw_event) {
            SDL_Delay(1);
        }
    }

    error = "timed out waiting for gamepad input";
    return std::nullopt;
}

}  // namespace trajectory::mapping
