#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <SDL3/SDL.h>

#include <ViGEm/Client.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "VirtualGamepadBridge.hpp"
#include "VirtualGamepadBridgeCli.hpp"

namespace {

struct EventHandlingResult {
    bool state_changed{false};
    bool forwarded_button_changed{false};
    SDL_GamepadButton button{SDL_GAMEPAD_BUTTON_INVALID};
};

// Shared shutdown flag set by console/signal handlers and observed by the main loop.
std::atomic<bool> g_should_stop = false;

BOOL WINAPI ConsoleControlHandler(DWORD control_type) {
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT || control_type == CTRL_CLOSE_EVENT) {
        g_should_stop = true;
        return TRUE;
    }
    return FALSE;
}

void SignalHandler(int) {
    g_should_stop = true;
}

std::uint64_t NowMonotonicNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::vector<std::string> CollectArguments(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? argc - 1 : 0);
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index] == nullptr ? "" : argv[index]);
    }
    return args;
}

std::string ProgramName(char* argv[]) {
    if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return "virtual_gamepad_bridge";
    }
    return argv[0];
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
    if (VIGEM_SUCCESS(error)) {
        return;
    }

    throw std::runtime_error(std::string(operation) + " failed: " + VigemErrorToString(error));
}

class SdlSession {
public:
    // The bridge must continue receiving controller events while a target game
    // window is focused, so background joystick events are enabled up front.
    SdlSession() {
        if (!SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1")) {
            throw std::runtime_error("failed to enable SDL background gamepad events");
        }
        if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    ~SdlSession() {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    }
};

class VigemSession {
public:
    // Owns one client connection and one virtual Xbox 360 target for the
    // lifetime of the sample process.
    VigemSession() {
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
    }

    ~VigemSession() {
        // Remove the target before freeing handles so Windows sees a clean
        // unplug event when the sample exits normally.
        if (target_ != nullptr && client_ != nullptr && vigem_target_is_attached(target_)) {
            vigem_target_remove(client_, target_);
        }
        if (target_ != nullptr) {
            vigem_target_free(target_);
        }
        if (client_ != nullptr) {
            vigem_disconnect(client_);
            vigem_free(client_);
        }
    }

    void Submit(const XUSB_REPORT& report) {
        ThrowIfVigemFailed("submitting XUSB report", vigem_target_x360_update(client_, target_, report));
    }

    unsigned long UserIndex() const {
        ULONG user_index = 0;
        const auto error = vigem_target_x360_get_user_index(client_, target_, &user_index);
        if (VIGEM_SUCCESS(error)) {
            return user_index;
        }
        return static_cast<unsigned long>(-1);
    }

private:
    PVIGEM_CLIENT client_{nullptr};
    PVIGEM_TARGET target_{nullptr};
};

class PhysicalGamepad {
public:
    ~PhysicalGamepad() {
        Close();
    }

    // Opens the first SDL-visible gamepad. The bridge is intentionally single-pad
    // for now so validation stays focused on the ViGEm integration path.
    bool OpenFirstAvailable() {
        if (gamepad_ != nullptr) {
            return true;
        }

        int count = 0;
        SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
        if (gamepads == nullptr) {
            return false;
        }

        for (int index = 0; index < count; ++index) {
            if (Open(gamepads[index])) {
                SDL_free(gamepads);
                return true;
            }
        }

        SDL_free(gamepads);
        return false;
    }

    bool Open(SDL_JoystickID instance_id) {
        if (gamepad_ != nullptr) {
            return true;
        }

        SDL_Gamepad* candidate = SDL_OpenGamepad(instance_id);
        if (candidate == nullptr) {
            return false;
        }

        gamepad_ = candidate;
        instance_id_ = instance_id;
        return true;
    }

    void CloseIfMatches(SDL_JoystickID instance_id) {
        if (gamepad_ != nullptr && instance_id_ == instance_id) {
            Close();
        }
    }

    bool IsOpen() const {
        return gamepad_ != nullptr;
    }

    SDL_JoystickID InstanceId() const {
        return instance_id_;
    }

private:
    void Close() {
        if (gamepad_ != nullptr) {
            SDL_CloseGamepad(gamepad_);
            gamepad_ = nullptr;
            instance_id_ = 0;
        }
    }

    SDL_Gamepad* gamepad_{nullptr};
    SDL_JoystickID instance_id_{0};
};

void PrintStartupBanner() {
    std::cout
        << "virtual_gamepad_bridge\n"
        << "Creates a virtual Xbox 360 controller via ViGEm and forwards SDL gamepad input to it.\n"
        << "Physical device hiding is not handled here. Use HidHide or equivalent before testing with games.\n"
        << "Press Ctrl+C to stop.\n"
        << std::endl;
}

EventHandlingResult HandleEvent(const SDL_Event& event,
                                PhysicalGamepad& physical_gamepad,
                                trajectory::virtual_gamepad::PhysicalGamepadState& state) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        g_should_stop = true;
        return {};
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!physical_gamepad.IsOpen() && physical_gamepad.Open(event.gdevice.which)) {
            std::cout << "Opened physical gamepad SDL instance " << event.gdevice.which << ".\n";
        }
        return {};
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (physical_gamepad.IsOpen() && physical_gamepad.InstanceId() == event.gdevice.which) {
            std::cout << "Physical gamepad removed.\n";
            physical_gamepad.CloseIfMatches(event.gdevice.which);
        }
        return {};
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        // Only forward the physical device currently owned by this process.
        if (!physical_gamepad.IsOpen() || event.gaxis.which != physical_gamepad.InstanceId()) {
            return {};
        }
        trajectory::virtual_gamepad::ApplyAxisMotion(state, static_cast<SDL_GamepadAxis>(event.gaxis.axis), event.gaxis.value);
        EventHandlingResult result;
        result.state_changed = true;
        return result;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    {
        if (!physical_gamepad.IsOpen() || event.gbutton.which != physical_gamepad.InstanceId()) {
            return {};
        }
        const SDL_GamepadButton button = static_cast<SDL_GamepadButton>(event.gbutton.button);
        trajectory::virtual_gamepad::ApplyButtonChange(
            state,
            button,
            event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        EventHandlingResult result;
        result.state_changed = true;
        result.forwarded_button_changed = true;
        result.button = button;
        return result;
    }
    default:
        return {};
    }
}

int Run(const trajectory::virtual_gamepad_bridge_cli::Options& options) {
    SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
    std::signal(SIGINT, SignalHandler);
    PrintStartupBanner();

    SdlSession sdl_session;
    VigemSession vigem_session;
    PhysicalGamepad physical_gamepad;
    trajectory::virtual_gamepad::PhysicalGamepadState state;
    trajectory::virtual_gamepad::ForwardingLogRateLimiter log_rate_limiter(options.max_forward_log_lines_per_second);

    if (physical_gamepad.OpenFirstAvailable()) {
        std::cout << "Opened physical gamepad SDL instance " << physical_gamepad.InstanceId() << ".\n";
    } else {
        std::cout << "No physical gamepad is open yet. Waiting for SDL_EVENT_GAMEPAD_ADDED.\n";
    }

    const unsigned long user_index = vigem_session.UserIndex();
    if (user_index != static_cast<unsigned long>(-1)) {
        std::cout << "Virtual Xbox 360 controller registered as XInput user index " << user_index << ".\n";
    } else {
        std::cout << "Virtual Xbox 360 controller registered.\n";
    }
    const std::string virtual_gamepad_id = user_index != static_cast<unsigned long>(-1) ? std::to_string(user_index) : "virtual";
    std::cout << "Forwarded-action logging capped at " << options.max_forward_log_lines_per_second << " lines/sec.\n";

    XUSB_REPORT previous_report = trajectory::virtual_gamepad::BuildXusbReport(state);
    vigem_session.Submit(previous_report);

    // The bridge is event-driven: SDL mutates the cached physical state, then
    // the loop rebuilds a full XUSB report and forwards it only when the final
    // report bytes actually changed. That keeps ViGEm traffic minimal while
    // still presenting a complete controller state to the virtual device.
    while (!g_should_stop.load()) {
        SDL_Event event;
        bool saw_event = false;
        while (SDL_PollEvent(&event)) {
            saw_event = true;
            const EventHandlingResult event_result = HandleEvent(event, physical_gamepad, state);
            if (event_result.state_changed) {
                const XUSB_REPORT next_report = trajectory::virtual_gamepad::BuildXusbReport(state);
                if (std::memcmp(&next_report, &previous_report, sizeof(XUSB_REPORT)) != 0) {
                    vigem_session.Submit(next_report);
                    previous_report = next_report;

                    if (event_result.forwarded_button_changed &&
                        log_rate_limiter.ShouldEmit(NowMonotonicNs())) {
                        std::cout << trajectory::virtual_gamepad::FormatForwardedButtonLogLine(
                                         physical_gamepad.InstanceId(),
                                         event_result.button,
                                         virtual_gamepad_id)
                                  << '\n';
                    }
                }
            }
        }

        if (!saw_event) {
            // Avoid a busy-spin when idle; new controller state only arrives
            // through future SDL events.
            SDL_Delay(1);
        }
    }

    std::cout << "Stopping bridge.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        trajectory::virtual_gamepad_bridge_cli::Options options;
        if (!trajectory::virtual_gamepad_bridge_cli::TryParseArguments(
                CollectArguments(argc, argv), ProgramName(argv), options, std::cout, std::cerr)) {
            return 1;
        }

        return Run(options);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
    return 1;
}
