#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "ActionMapping.hpp"

struct SDL_Gamepad;

namespace trajectory::mapping {

// Represents one binding candidate observed from live SDL gamepad input.
struct ObservedBinding {
    ActionBinding binding;
    std::string label;
};

// Owns SDL gamepad setup for the mapper and samples raw bindings on demand.
// Fields:
// - `started_`: tracks whether SDL input capture is currently initialized.
// - `gamepad_`: currently opened SDL gamepad used for event-driven sampling.
class GamepadBindingCapture {
public:
    // Constructs an idle capture session. SDL is initialized by `Start()`.
    GamepadBindingCapture();

    // Shuts down any open SDL gamepad/session state owned by the mapper.
    ~GamepadBindingCapture();

    // Initializes SDL input capture and opens the first available gamepad.
    void Start();

    // Releases the opened gamepad and shuts down the SDL subsystems used by the mapper.
    void Stop();

    // Waits for the next binding candidate that matches the expected action kind.
    // On timeout or setup failure, returns `std::nullopt` and writes a user-facing error.
    std::optional<ObservedBinding> WaitForBinding(ActionInputKind kind,
                                                  std::chrono::milliseconds timeout,
                                                  std::string& error);

private:
    bool started_{false};
    SDL_Gamepad* gamepad_{nullptr};
};

}  // namespace trajectory::mapping
