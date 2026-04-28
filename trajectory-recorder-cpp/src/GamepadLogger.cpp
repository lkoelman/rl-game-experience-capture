#include "GamepadLogger.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ViGEm/Client.h>

#include "BinaryIO.hpp"
#include "VirtualGamepadBridge.hpp"

namespace trajectory {

namespace {

// Uses the same monotonic clock source as VideoRecorder for offline alignment.
std::uint64_t NowMonotonicNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::string VigemErrorToString(VIGEM_ERROR error) {
    switch (error) {
    case VIGEM_ERROR_NONE:
        return "VIGEM_ERROR_NONE";
    case VIGEM_ERROR_BUS_NOT_FOUND:
        return "VIGEM_ERROR_BUS_NOT_FOUND";
    case VIGEM_ERROR_NO_FREE_SLOT:
        return "VIGEM_ERROR_NO_FREE_SLOT";
    case VIGEM_ERROR_INVALID_TARGET:
        return "VIGEM_ERROR_INVALID_TARGET";
    case VIGEM_ERROR_REMOVAL_FAILED:
        return "VIGEM_ERROR_REMOVAL_FAILED";
    case VIGEM_ERROR_ALREADY_CONNECTED:
        return "VIGEM_ERROR_ALREADY_CONNECTED";
    case VIGEM_ERROR_TARGET_UNINITIALIZED:
        return "VIGEM_ERROR_TARGET_UNINITIALIZED";
    case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN:
        return "VIGEM_ERROR_TARGET_NOT_PLUGGED_IN";
    case VIGEM_ERROR_BUS_VERSION_MISMATCH:
        return "VIGEM_ERROR_BUS_VERSION_MISMATCH";
    case VIGEM_ERROR_BUS_ACCESS_FAILED:
        return "VIGEM_ERROR_BUS_ACCESS_FAILED";
    case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED:
        return "VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED";
    case VIGEM_ERROR_CALLBACK_NOT_FOUND:
        return "VIGEM_ERROR_CALLBACK_NOT_FOUND";
    case VIGEM_ERROR_BUS_ALREADY_CONNECTED:
        return "VIGEM_ERROR_BUS_ALREADY_CONNECTED";
    case VIGEM_ERROR_BUS_INVALID_HANDLE:
        return "VIGEM_ERROR_BUS_INVALID_HANDLE";
    case VIGEM_ERROR_XUSB_USERINDEX_OUT_OF_RANGE:
        return "VIGEM_ERROR_XUSB_USERINDEX_OUT_OF_RANGE";
    case VIGEM_ERROR_INVALID_PARAMETER:
        return "VIGEM_ERROR_INVALID_PARAMETER";
    case VIGEM_ERROR_NOT_SUPPORTED:
        return "VIGEM_ERROR_NOT_SUPPORTED";
    case VIGEM_ERROR_WINAPI:
        return "VIGEM_ERROR_WINAPI";
    case VIGEM_ERROR_TIMED_OUT:
        return "VIGEM_ERROR_TIMED_OUT";
    case VIGEM_ERROR_IS_DISPOSING:
        return "VIGEM_ERROR_IS_DISPOSING";
    }

    return "VIGEM_ERROR_UNKNOWN";
}

void ThrowIfVigemFailed(const char* operation, VIGEM_ERROR error) {
    if (!VIGEM_SUCCESS(error)) {
        throw std::runtime_error(std::string(operation) + " failed: " + VigemErrorToString(error));
    }
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

GamepadLogger::GamepadLogger(const std::string& output_path, bool verbose) : output_path_(output_path), verbose_(verbose) {
    state_mutex_ = SDL_CreateMutex();
    if (state_mutex_ == nullptr) {
        throw std::runtime_error("failed to create SDL mutex");
    }
}

class GamepadLogger::VirtualGamepadForwarder {
public:
    // Brings up the recorder-owned virtual Xbox pad before recording starts.
    // This is the concrete PRD workaround for "logger sees input, game does
    // not": the game should play against the virtual pad while the recorder
    // continues to read the physical one through SDL.
    void Start() {
        client_ = vigem_alloc();
        if (client_ == nullptr) {
            throw std::runtime_error("failed to allocate ViGEm client");
        }

        ThrowIfVigemFailed("connecting to ViGEm bus", vigem_connect(client_));

        target_ = vigem_target_x360_alloc();
        if (target_ == nullptr) {
            throw std::runtime_error("failed to allocate Xbox 360 target");
        }

        ThrowIfVigemFailed("adding Xbox 360 target", vigem_target_add(client_, target_));
        SubmitState(state_);
    }

    // Explicitly tears down the virtual pad so Windows sees a clean unplug and
    // the recorder does not leave behind a stale virtual controller after exit.
    void Stop() {
        if (target_ != nullptr && client_ != nullptr && vigem_target_is_attached(target_)) {
            vigem_target_remove(client_, target_);
        }
        if (target_ != nullptr) {
            vigem_target_free(target_);
            target_ = nullptr;
        }
        if (client_ != nullptr) {
            vigem_disconnect(client_);
            vigem_free(client_);
            client_ = nullptr;
        }
        XUSB_REPORT_INIT(&previous_report_);
        has_previous_report_ = false;
        state_ = {};
    }

    bool ApplyAxisMotion(SDL_GamepadAxis axis, short value) {
        virtual_gamepad::ApplyAxisMotion(state_, axis, value);
        return SubmitState(state_);
    }

    bool ApplyButtonChange(SDL_GamepadButton button, bool pressed) {
        virtual_gamepad::ApplyButtonChange(state_, button, pressed);
        return SubmitState(state_);
    }

    // Device removal should release all virtual inputs immediately. Without
    // this reset, the game could continue seeing a stuck stick/button state.
    bool ResetPhysicalState() {
        state_ = {};
        return SubmitState(state_);
    }

    std::string VirtualGamepadId() const {
        ULONG user_index = 0;
        if (client_ != nullptr && target_ != nullptr &&
            VIGEM_SUCCESS(vigem_target_x360_get_user_index(client_, target_, &user_index))) {
            return std::to_string(user_index);
        }
        return "virtual";
    }

    ~VirtualGamepadForwarder() {
        Stop();
    }

private:
    bool SubmitState(const virtual_gamepad::PhysicalGamepadState& state) {
        if (client_ == nullptr || target_ == nullptr) {
            return false;
        }

        const XUSB_REPORT next_report = virtual_gamepad::BuildXusbReport(state);
        // ViGEm traffic stays event-driven and minimal: we submit only when the
        // full virtual report bytes changed after an SDL mutation.
        if (has_previous_report_ && !virtual_gamepad::ReportsDiffer(previous_report_, next_report)) {
            return false;
        }

        ThrowIfVigemFailed("submitting XUSB report", vigem_target_x360_update(client_, target_, next_report));
        previous_report_ = next_report;
        has_previous_report_ = true;
        return true;
    }

    PVIGEM_CLIENT client_{nullptr};
    PVIGEM_TARGET target_{nullptr};
    virtual_gamepad::PhysicalGamepadState state_{};
    XUSB_REPORT previous_report_{};
    bool has_previous_report_{false};
};

GamepadLogger::~GamepadLogger() {
    Stop();
    if (state_mutex_ != nullptr) {
        SDL_DestroyMutex(state_mutex_);
    }
}

void GamepadLogger::Start() {
    StartForwarding();
    BeginRecording();
}

void GamepadLogger::StartForwarding() {
    if (is_forwarding_.exchange(true)) {
        return;
    }

    // The recorder usually loses focus once the game window becomes active.
    // Background controller events are therefore required for both logging and
    // forwarding to continue while the game is being played.
    if (!SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1")) {
        is_forwarding_ = false;
        throw std::runtime_error("failed to enable SDL background gamepad events");
    }

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        is_forwarding_ = false;
        throw std::runtime_error(SDL_GetError());
    }

    try {
        // Bring up the virtual controller during recorder startup so the game
        // can bind to it for the entire recording session.
        virtual_gamepad_forwarder_ = std::make_unique<VirtualGamepadForwarder>();
        virtual_gamepad_forwarder_->Start();
    } catch (...) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
        is_forwarding_ = false;
        virtual_gamepad_forwarder_.reset();
        throw;
    }

    // Open an already-connected controller immediately so the operator does not
    // need to replug it after starting the recorder.
    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    if (gamepads != nullptr) {
        for (int index = 0; index < gamepad_count; ++index) {
            gamepad_ = SDL_OpenGamepad(gamepads[index]);
            if (gamepad_ != nullptr) {
                gamepad_instance_id_ = gamepads[index];
                break;
            }
        }
        SDL_free(gamepads);
    }
}

void GamepadLogger::BeginRecording() {
    if (!is_forwarding_) {
        StartForwarding();
    }
    if (is_recording_) {
        return;
    }

    out_bin_.open(output_path_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out_bin_.is_open()) {
        throw std::runtime_error("failed to open actions binary: " + output_path_);
    }

    GamepadState initial_snapshot;
    SDL_LockMutex(state_mutex_);
    initial_snapshot = SnapshotState(NowMonotonicNs());
    SDL_UnlockMutex(state_mutex_);
    WriteState(initial_snapshot);
    if (verbose_) {
        std::cout << FormatVerboseState(initial_snapshot) << std::endl;
    }
    is_recording_ = true;
}

GamepadPumpResult GamepadLogger::PumpEventsOnce(GamepadPumpMode mode) {
    GamepadPumpResult pump_result;
    if (!is_forwarding_) {
        return pump_result;
    }

    static virtual_gamepad::ForwardingLogRateLimiter preview_log_rate_limiter(30);

    SDL_Event event;
    bool saw_event = false;
    // Drain the SDL queue on the main thread.
    // SDL gives us transitions such as "axis moved" or "button released".
    // We translate each transition into updates on our cached full state.
    while (SDL_PollEvent(&event)) {
        saw_event = true;
        bool state_changed = false;
        bool forwarded_report_changed = false;
        bool forwarded_button_changed = false;
        SDL_GamepadButton forwarded_button = SDL_GAMEPAD_BUTTON_INVALID;

        SDL_LockMutex(state_mutex_);
        switch (event.type) {
        case SDL_EVENT_QUIT:
            pump_result.shutdown_requested = true;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            // Track one controller. The logger records state, not per-device identity.
            if (gamepad_ == nullptr) {
                gamepad_ = SDL_OpenGamepad(event.gdevice.which);
                if (gamepad_ != nullptr) {
                    // Forwarding and logging must agree on exactly one physical
                    // source device, otherwise multiple pads could be merged
                    // into one action stream and one virtual pad.
                    gamepad_instance_id_ = event.gdevice.which;
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            // Device removal stops future updates but does not emit a synthetic "all released" snapshot.
            if (gamepad_ != nullptr && gamepad_instance_id_ == event.gdevice.which) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
                gamepad_instance_id_ = 0;
                std::fill(std::begin(axes_), std::end(axes_), 0.0f);
                pressed_buttons_.clear();
                // The virtual pad must also return to neutral on disconnect so
                // games do not keep receiving the last observed physical state.
                if (virtual_gamepad_forwarder_ != nullptr) {
                    virtual_gamepad_forwarder_->ResetPhysicalState();
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            // Ignore non-owned devices. The recorder intentionally models one
            // physical pad -> one virtual pad -> one serialized action stream.
            if (gamepad_ == nullptr || event.gaxis.which != gamepad_instance_id_) {
                break;
            }
            // SDL reports signed 16-bit axis motion.
            // We normalize to roughly [-1, 1] and store it by SDL_GamepadAxis index.
            // SnapshotState later copies the whole array so readers can treat `axes[i]`
            // as "latest known value for axis i" without replaying intermediate events.
            axes_[event.gaxis.axis] = static_cast<float>(event.gaxis.value) / 32767.0f;
            if (virtual_gamepad_forwarder_ != nullptr) {
                // The game sees the forwarded virtual state, while actions.bin
                // still records the observed physical state for offline use.
                forwarded_report_changed =
                    virtual_gamepad_forwarder_->ApplyAxisMotion(static_cast<SDL_GamepadAxis>(event.gaxis.axis), event.gaxis.value);
            }
            state_changed = true;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (gamepad_ == nullptr || event.gbutton.which != gamepad_instance_id_) {
                break;
            }
            // Store pressed buttons as SDL_GamepadButton enum ids.
            forwarded_button = static_cast<SDL_GamepadButton>(event.gbutton.button);
            if (virtual_gamepad_forwarder_ != nullptr) {
                forwarded_report_changed = virtual_gamepad_forwarder_->ApplyButtonChange(forwarded_button, true);
            }
            forwarded_button_changed = true;
            state_changed = pressed_buttons_.insert(event.gbutton.button).second;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (gamepad_ == nullptr || event.gbutton.which != gamepad_instance_id_) {
                break;
            }
            forwarded_button = static_cast<SDL_GamepadButton>(event.gbutton.button);
            if (virtual_gamepad_forwarder_ != nullptr) {
                forwarded_report_changed = virtual_gamepad_forwarder_->ApplyButtonChange(forwarded_button, false);
            }
            forwarded_button_changed = true;
            state_changed = pressed_buttons_.erase(event.gbutton.button) > 0;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (mode == GamepadPumpMode::preview && !event.key.repeat && event.key.scancode == SDL_SCANCODE_SPACE) {
                pump_result.start_recording_requested = true;
                break;
            }
            // Keyboard keys are fallback/auxiliary controls stored as SDL_Scancode ids.
            if (mode == GamepadPumpMode::recording && !event.key.repeat) {
                state_changed = pressed_keys_.insert(event.key.scancode).second;
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (mode == GamepadPumpMode::recording) {
                state_changed = pressed_keys_.erase(event.key.scancode) > 0;
            }
            break;
        default:
            break;
        }
        GamepadState snapshot;
        if (state_changed) {
            // Persist a full snapshot after each mutation.
            // The protobuf is a state sample: timestamp + all axes + pressed buttons + pressed keys.
            // Offline alignment can therefore do timestamp lookup only; it does not need to replay SDL events.
            //
            // Limitation: this is not fixed-rate sampling.
            // If the input state stays constant, no new record is written for that time span.
            // Very short transitions can also be missed if SDL never emits them into this process.
            snapshot = SnapshotState(NowMonotonicNs());
        }
        SDL_UnlockMutex(state_mutex_);

        if (mode == GamepadPumpMode::preview && forwarded_report_changed && forwarded_button_changed &&
            virtual_gamepad_forwarder_ != nullptr && preview_log_rate_limiter.ShouldEmit(NowMonotonicNs())) {
            std::cout << virtual_gamepad::FormatForwardedButtonLogLine(
                             gamepad_instance_id_,
                             forwarded_button,
                             virtual_gamepad_forwarder_->VirtualGamepadId())
                      << std::endl;
        }

        if (mode == GamepadPumpMode::recording && is_recording_ && state_changed) {
            WriteState(snapshot);
            if (verbose_) {
                std::cout << FormatVerboseState(snapshot) << std::endl;
            }
        }
    }

    if (!saw_event) {
        // Yield when idle.
        // No event means no new protobuf snapshot, so long idle periods appear as gaps between records.
        SDL_Delay(1);
    }

    return pump_result;
}

void GamepadLogger::PumpEventsOnce() {
    static_cast<void>(PumpEventsOnce(GamepadPumpMode::recording));
}

void GamepadLogger::Stop() {
    if (!is_forwarding_.exchange(false)) {
        return;
    }

    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        gamepad_instance_id_ = 0;
    }
    // Destroy the forwarder before SDL teardown so ViGEm cleanup can still use
    // the final cached controller state and produce a clean virtual unplug.
    virtual_gamepad_forwarder_.reset();
    if (out_bin_.is_open()) {
        out_bin_.close();
    }
    is_recording_ = false;
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
}

void GamepadLogger::WriteState(const GamepadState& state) {
    std::string payload;
    if (!state.SerializeToString(&payload)) {
        throw std::runtime_error("failed to serialize gamepad state");
    }
    WriteLengthPrefixedPayload(out_bin_, payload);
}

GamepadState GamepadLogger::SnapshotState(std::uint64_t monotonic_ns) const {
    GamepadState state;
    state.set_monotonic_ns(monotonic_ns);

    // Preserve SDL axis ordering exactly.
    // Readers interpret `axes[n]` using the SDL_GamepadAxis enum value `n`.
    for (float axis : axes_) {
        state.add_axes(axis);
    }

    // Sets are sorted before serialization so equal states encode deterministically.
    // Each value is an SDL_GamepadButton enum id that is currently held down.
    std::vector<std::uint32_t> buttons(pressed_buttons_.begin(), pressed_buttons_.end());
    std::sort(buttons.begin(), buttons.end());
    for (const auto button : buttons) {
        state.add_pressed_buttons(button);
    }

    // Each value is an SDL_Scancode currently held down.
    std::vector<std::uint32_t> keys(pressed_keys_.begin(), pressed_keys_.end());
    std::sort(keys.begin(), keys.end());
    for (const auto key : keys) {
        state.add_pressed_keys(key);
    }

    return state;
}

}  // namespace trajectory
