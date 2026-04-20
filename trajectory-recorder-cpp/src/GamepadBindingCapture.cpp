#include "GamepadBindingCapture.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <SDL3/SDL.h>

namespace trajectory::mapping {

namespace {

float NormalizeAxisValue(Sint16 value) {
    const float normalized = static_cast<float>(value) / 32767.0f;
    return std::clamp(normalized, -1.0f, 1.0f);
}

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

bool IsAnalogAxis(SDL_GamepadAxis axis) {
    return axis == SDL_GAMEPAD_AXIS_LEFTX || axis == SDL_GAMEPAD_AXIS_LEFTY ||
           axis == SDL_GAMEPAD_AXIS_RIGHTX || axis == SDL_GAMEPAD_AXIS_RIGHTY;
}

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
    ClearObservedBindings();
}

void GamepadBindingCapture::Stop() {
    if (!started_) {
        return;
    }
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    ClearObservedBindings();
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    started_ = false;
}

std::optional<ObservedBinding> GamepadBindingCapture::PollBinding(ActionInputKind kind) {
    if (!started_) {
        return std::nullopt;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
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
                ClearObservedBindings();
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
            const std::string control = ButtonControlName(static_cast<SDL_GamepadButton>(event.gbutton.button));
            if (!control.empty()) {
                digital_binding_ = ObservedBinding{ActionBinding::Button(control), "button " + control};
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            const std::string control = ButtonControlName(static_cast<SDL_GamepadButton>(event.gbutton.button));
            if (digital_binding_.has_value() && digital_binding_->binding.control == control) {
                digital_binding_.reset();
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const SDL_GamepadAxis axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
            const std::string control = AxisControlName(axis);
            if (control.empty()) {
                break;
            }

            const float value = NormalizeAxisValue(event.gaxis.value);
            if (IsAnalogAxis(axis)) {
                if (std::fabs(value) >= 0.35f) {
                    analog_binding_ = ObservedBinding{ActionBinding::Axis(control, "any"), "axis " + control};
                } else if (analog_binding_.has_value() && analog_binding_->binding.control == control) {
                    analog_binding_.reset();
                }
            }

            if (IsTriggerAxis(axis)) {
                if (value >= 0.2f) {
                    const float threshold = std::clamp(value * 0.8f, 0.2f, 1.0f);
                    trigger_binding_ = ObservedBinding{ActionBinding::Trigger(control, threshold), "trigger " + control};
                } else if (trigger_binding_.has_value() && trigger_binding_->binding.control == control) {
                    trigger_binding_.reset();
                }
            }
            break;
        }
        default:
            break;
        }
    }

    switch (kind) {
    case ActionInputKind::digital:
        return digital_binding_;
    case ActionInputKind::analog:
        return analog_binding_;
    case ActionInputKind::trigger:
        return trigger_binding_;
    }
    return std::nullopt;
}

void GamepadBindingCapture::ClearObservedBindings() {
    digital_binding_.reset();
    analog_binding_.reset();
    trigger_binding_.reset();
}

}  // namespace trajectory::mapping
