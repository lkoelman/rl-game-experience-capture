#include "GamepadBindingCapture.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

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

std::string StickControlName(SDL_GamepadAxis axis) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
    case SDL_GAMEPAD_AXIS_LEFTY:
        return "left_stick";
    case SDL_GAMEPAD_AXIS_RIGHTX:
    case SDL_GAMEPAD_AXIS_RIGHTY:
        return "right_stick";
    default:
        return {};
    }
}

float StickMagnitude(const std::unordered_map<std::string, float>& axis_values, const std::string& stick_control) {
    const std::string x_axis = stick_control == "left_stick" ? "leftx" : "rightx";
    const std::string y_axis = stick_control == "left_stick" ? "lefty" : "righty";
    const float x = axis_values.contains(x_axis) ? axis_values.at(x_axis) : 0.0f;
    const float y = axis_values.contains(y_axis) ? axis_values.at(y_axis) : 0.0f;
    return std::sqrt((x * x) + (y * y));
}

std::string AxisDirection(float value) {
    return value >= 0.0f ? "positive" : "negative";
}

bool IsAxisButtonActive(const std::string& control, float value, float threshold) {
    if (control == "left_trigger" || control == "right_trigger") {
        return value >= threshold;
    }
    return std::fabs(value) >= threshold;
}

struct DigitalObservation {
    ObservedBinding observed;
    std::size_t component_count{0};
};

DigitalObservation BuildDigitalObservedBinding(const std::unordered_set<std::string>& pressed_buttons,
                                               const std::unordered_map<std::string, float>& axis_values,
                                               const ActionMappingProfile& profile,
                                               int max_combo_buttons,
                                               std::string& warning) {
    std::vector<ComboComponent> components;
    std::vector<std::string> labels;

    for (const auto& button : pressed_buttons) {
        components.push_back(ComboComponent::Button(button));
        labels.push_back("button " + button);
    }

    for (const auto& axis_threshold : NormalizeAxisButtonThresholds(profile.axis_button_thresholds)) {
        const auto axis_it = axis_values.find(axis_threshold.control);
        const float value = axis_it == axis_values.end() ? 0.0f : axis_it->second;
        if (!IsAxisButtonActive(axis_threshold.control, value, axis_threshold.threshold)) {
            continue;
        }

        const std::string direction = (axis_threshold.control == "left_trigger" || axis_threshold.control == "right_trigger")
                                          ? ""
                                          : AxisDirection(value);
        components.push_back(ComboComponent::AxisButton(axis_threshold.control, direction));
        labels.push_back(direction.empty() ? "axis " + axis_threshold.control
                                           : "axis " + axis_threshold.control + " " + direction);
    }

    if (components.empty()) {
        warning.clear();
        return DigitalObservation{};
    }

    std::sort(components.begin(), components.end(), [](const ComboComponent& left, const ComboComponent& right) {
        if (left.control != right.control) {
            return left.control < right.control;
        }
        if (left.direction != right.direction) {
            return left.direction < right.direction;
        }
        return static_cast<int>(left.type) < static_cast<int>(right.type);
    });
    std::sort(labels.begin(), labels.end());

    if (static_cast<int>(components.size()) > max_combo_buttons) {
        warning = "Too many simultaneous controls detected. Maximum allowed is " + std::to_string(max_combo_buttons) + ".";
        return DigitalObservation{ObservedBinding{}, components.size()};
    }

    warning.clear();

    std::ostringstream label;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (index > 0) {
            label << " + ";
        }
        label << labels[index];
    }

    if (components.size() == 1 && components[0].type == ComboComponentType::button) {
        return DigitalObservation{
            ObservedBinding{ActionBinding::Button(components[0].control), label.str()},
            1,
        };
    }

    return DigitalObservation{
        ObservedBinding{ActionBinding::Combo(std::move(components)), label.str()},
        components.size(),
    };
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

std::optional<ObservedBinding> GamepadBindingCapture::PollBinding(ActionInputKind kind,
                                                                  const ActionMappingProfile& profile,
                                                                  int max_combo_buttons) {
    if (!started_) {
        return std::nullopt;
    }

    SDL_Event event;
    bool digital_state_changed = false;
    bool saw_release_only_change = false;
    bool saw_press_like_change = false;
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
                pressed_buttons_.insert(control);
                digital_state_changed = true;
                saw_press_like_change = true;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            const std::string control = ButtonControlName(static_cast<SDL_GamepadButton>(event.gbutton.button));
            if (!control.empty()) {
                pressed_buttons_.erase(control);
                digital_state_changed = true;
                if (!saw_press_like_change) {
                    saw_release_only_change = true;
                }
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
            const float previous_value = axis_values_.contains(control) ? axis_values_.at(control) : 0.0f;
            axis_values_[control] = value;
            digital_state_changed = true;
            const float threshold = ResolveAxisButtonThreshold(profile, control);
            const bool was_active = IsAxisButtonActive(control, previous_value, threshold);
            const bool is_active = IsAxisButtonActive(control, value, threshold);
            if (is_active && !was_active) {
                saw_press_like_change = true;
            }
            if (!is_active && was_active && !saw_press_like_change) {
                saw_release_only_change = true;
            }

            if (IsAnalogAxis(axis) && std::fabs(value) >= 0.35f) {
                analog_binding_ = ObservedBinding{ActionBinding::Axis(control, "any"), "axis " + control};
            }

            if (IsAnalogAxis(axis)) {
                const std::string stick_control = StickControlName(axis);
                if (!stick_control.empty() && StickMagnitude(axis_values_, stick_control) >= 0.45f) {
                    vector2_binding_ = ObservedBinding{ActionBinding::Stick(stick_control), "stick " + stick_control};
                }
            }

            if (IsTriggerAxis(axis) && value >= 0.2f) {
                const float trigger_threshold = std::clamp(value * 0.8f, 0.2f, 1.0f);
                trigger_binding_ = ObservedBinding{ActionBinding::Trigger(control, trigger_threshold), "trigger " + control};
            }
            break;
        }
        default:
            break;
        }
    }

    if (digital_state_changed) {
        const DigitalObservation observation = BuildDigitalObservedBinding(pressed_buttons_, axis_values_, profile, max_combo_buttons, current_warning_);
        if (!observation.observed.label.empty()) {
            if (saw_press_like_change || !saw_release_only_change || !digital_binding_.has_value() || !current_warning_.empty()) {
                digital_binding_ = observation.observed;
            }
        } else if (!current_warning_.empty()) {
            digital_binding_.reset();
        }
    }

    switch (kind) {
    case ActionInputKind::digital:
        return digital_binding_;
    case ActionInputKind::analog:
        return analog_binding_;
    case ActionInputKind::vector2:
        return vector2_binding_;
    case ActionInputKind::trigger:
        return trigger_binding_;
    }
    return std::nullopt;
}

const std::string& GamepadBindingCapture::CurrentWarning() const {
    return current_warning_;
}

void GamepadBindingCapture::ClearObservedBindings() {
    pressed_buttons_.clear();
    axis_values_.clear();
    digital_binding_.reset();
    analog_binding_.reset();
    vector2_binding_.reset();
    trigger_binding_.reset();
    current_warning_.clear();
}

}  // namespace trajectory::mapping
