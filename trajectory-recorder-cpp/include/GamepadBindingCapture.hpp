#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "ActionMapping.hpp"

struct SDL_Gamepad;

namespace trajectory::mapping {

struct ObservedBinding {
    ActionBinding binding;
    std::string label;
};

class GamepadBindingCapture {
public:
    GamepadBindingCapture();
    ~GamepadBindingCapture();

    void Start();
    void Stop();

    std::optional<ObservedBinding> WaitForBinding(ActionInputKind kind,
                                                  std::chrono::milliseconds timeout,
                                                  std::string& error);

private:
    bool started_{false};
    SDL_Gamepad* gamepad_{nullptr};
};

}  // namespace trajectory::mapping
