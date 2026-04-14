#include "InputLogger.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "BinaryIO.hpp"

namespace trajectory {

namespace {

// Uses the same monotonic clock source as VideoRecorder for offline alignment.
std::uint64_t NowMonotonicNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

std::string FormatVerboseState(const GamepadState& state) {
    std::ostringstream formatted;
    formatted << "input monotonic_ns=" << state.monotonic_ns() << " axes=[";
    for (int index = 0; index < state.axes_size(); ++index) {
        if (index > 0) {
            formatted << ',';
        }
        formatted << state.axes(index);
    }
    formatted << "] buttons=[";
    for (int index = 0; index < state.pressed_buttons_size(); ++index) {
        if (index > 0) {
            formatted << ',';
        }
        formatted << state.pressed_buttons(index);
    }
    formatted << "] keys=[";
    for (int index = 0; index < state.pressed_keys_size(); ++index) {
        if (index > 0) {
            formatted << ',';
        }
        formatted << state.pressed_keys(index);
    }
    formatted << ']';
    return formatted.str();
}

InputLogger::InputLogger(const std::string& output_path, bool verbose) : output_path_(output_path), verbose_(verbose) {
    state_mutex_ = SDL_CreateMutex();
    if (state_mutex_ == nullptr) {
        throw std::runtime_error("failed to create SDL mutex");
    }
}

InputLogger::~InputLogger() {
    Stop();
    if (state_mutex_ != nullptr) {
        SDL_DestroyMutex(state_mutex_);
    }
}

void InputLogger::Start() {
    if (is_running_.exchange(true)) {
        return;
    }

    out_bin_.open(output_path_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out_bin_.is_open()) {
        is_running_ = false;
        throw std::runtime_error("failed to open actions binary: " + output_path_);
    }

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        out_bin_.close();
        is_running_ = false;
        throw std::runtime_error(SDL_GetError());
    }

}

void InputLogger::PumpEventsOnce() {
    if (!is_running_) {
        return;
    }

    SDL_Event event;
    bool saw_event = false;
    while (SDL_PollEvent(&event)) {
        saw_event = true;
        bool state_changed = false;

        SDL_LockMutex(state_mutex_);
        switch (event.type) {
        case SDL_EVENT_QUIT:
            is_running_ = false;
            break;
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
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            axes_[event.gaxis.axis] = static_cast<float>(event.gaxis.value) / 32767.0f;
            state_changed = true;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            state_changed = pressed_buttons_.insert(event.gbutton.button).second;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            state_changed = pressed_buttons_.erase(event.gbutton.button) > 0;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                state_changed = pressed_keys_.insert(event.key.scancode).second;
            }
            break;
        case SDL_EVENT_KEY_UP:
            state_changed = pressed_keys_.erase(event.key.scancode) > 0;
            break;
        default:
            break;
        }
        GamepadState snapshot;
        if (state_changed) {
            // Persist a full snapshot after every input mutation so replay only needs timestamp lookup.
            snapshot = SnapshotState(NowMonotonicNs());
        }
        SDL_UnlockMutex(state_mutex_);

        if (state_changed) {
            WriteState(snapshot);
            if (verbose_) {
                std::cout << FormatVerboseState(snapshot) << std::endl;
            }
        }
    }

    if (!saw_event) {
        // Yield when idle; this loop is event-driven rather than fixed-rate sampled.
        SDL_Delay(1);
    }
}

void InputLogger::Stop() {
    if (!is_running_.exchange(false)) {
        return;
    }

    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    if (out_bin_.is_open()) {
        out_bin_.close();
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
}

void InputLogger::WriteState(const GamepadState& state) {
    std::string payload;
    if (!state.SerializeToString(&payload)) {
        throw std::runtime_error("failed to serialize gamepad state");
    }
    WriteLengthPrefixedPayload(out_bin_, payload);
}

GamepadState InputLogger::SnapshotState(std::uint64_t monotonic_ns) const {
    GamepadState state;
    state.set_monotonic_ns(monotonic_ns);

    for (float axis : axes_) {
        state.add_axes(axis);
    }

    std::vector<std::uint32_t> buttons(pressed_buttons_.begin(), pressed_buttons_.end());
    std::sort(buttons.begin(), buttons.end());
    for (const auto button : buttons) {
        state.add_pressed_buttons(button);
    }

    std::vector<std::uint32_t> keys(pressed_keys_.begin(), pressed_keys_.end());
    std::sort(keys.begin(), keys.end());
    for (const auto key : keys) {
        state.add_pressed_keys(key);
    }

    return state;
}

}  // namespace trajectory
