#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <SDL3/SDL.h>

#include <ViGEm/Client.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "VirtualGamepadBridge.hpp"

namespace {

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

bool HandleEvent(const SDL_Event& event,
                 PhysicalGamepad& physical_gamepad,
                 trajectory::virtual_gamepad::PhysicalGamepadState& state) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        g_should_stop = true;
        return false;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!physical_gamepad.IsOpen() && physical_gamepad.Open(event.gdevice.which)) {
            std::cout << "Opened physical gamepad SDL instance " << event.gdevice.which << ".\n";
        }
        return false;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (physical_gamepad.IsOpen() && physical_gamepad.InstanceId() == event.gdevice.which) {
            std::cout << "Physical gamepad removed.\n";
            physical_gamepad.CloseIfMatches(event.gdevice.which);
        }
        return false;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (!physical_gamepad.IsOpen() || event.gaxis.which != physical_gamepad.InstanceId()) {
            return false;
        }
        trajectory::virtual_gamepad::ApplyAxisMotion(state, static_cast<SDL_GamepadAxis>(event.gaxis.axis), event.gaxis.value);
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (!physical_gamepad.IsOpen() || event.gbutton.which != physical_gamepad.InstanceId()) {
            return false;
        }
        trajectory::virtual_gamepad::ApplyButtonChange(
            state,
            static_cast<SDL_GamepadButton>(event.gbutton.button),
            event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        return true;
    default:
        return false;
    }
}

int Run() {
    SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
    std::signal(SIGINT, SignalHandler);
    PrintStartupBanner();

    SdlSession sdl_session;
    VigemSession vigem_session;
    PhysicalGamepad physical_gamepad;
    trajectory::virtual_gamepad::PhysicalGamepadState state;

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

    XUSB_REPORT previous_report = trajectory::virtual_gamepad::BuildXusbReport(state);
    vigem_session.Submit(previous_report);

    while (!g_should_stop.load()) {
        SDL_Event event;
        bool saw_event = false;
        while (SDL_PollEvent(&event)) {
            saw_event = true;
            if (HandleEvent(event, physical_gamepad, state)) {
                const XUSB_REPORT next_report = trajectory::virtual_gamepad::BuildXusbReport(state);
                if (std::memcmp(&next_report, &previous_report, sizeof(XUSB_REPORT)) != 0) {
                    vigem_session.Submit(next_report);
                    previous_report = next_report;
                }
            }
        }

        if (!saw_event) {
            SDL_Delay(1);
        }
    }

    std::cout << "Stopping bridge.\n";
    return 0;
}

}  // namespace

int main() {
    try {
        return Run();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
    return 1;
}
