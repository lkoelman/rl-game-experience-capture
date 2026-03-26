#include "sdl_input_recorder.h"
#include <chrono>
#include <iostream>

SdlInputRecorder::SdlInputRecorder()
    : recording_(false)
    , controller_(nullptr)
{
    // Initialize SDL GameController subsystem
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL GameController init failed: " << SDL_GetError() << "\n";
        return;
    }
}

SdlInputRecorder::~SdlInputRecorder() {
    StopRecording();
    
    if (controller_) {
        SDL_GameControllerClose(controller_);
    }
    
    SDL_Quit();
}

bool SdlInputRecorder::StartRecording(const std::string& output_file) {
    if (recording_) {
        return false;
    }

    // Try to find and open first available controller
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                break;
            }
        }
    }

    if (!controller_) {
        std::cout << "Warning: No game controllers detected!\n";
    }

    output_file_ = output_file;
    csv_file_.open(output_file_, std::ios::out);
    if (!csv_file_.is_open()) {
        return false;
    }

    // Write CSV header
    csv_file_ << "timestamp,controller_type,buttons,"
              << "left_stick_x,left_stick_y,"
              << "right_stick_x,right_stick_y,"
              << "left_trigger,right_trigger,"
              << "dpad_up,dpad_down,dpad_left,dpad_right\n";
    
    recording_ = true;
    return true;
}

void SdlInputRecorder::StopRecording() {
    if (!recording_) {
        return;
    }

    recording_ = false;
    csv_file_.close();
}

void SdlInputRecorder::Update() {
    if (!recording_ || !controller_) {
        return;
    }

    // Process SDL events to keep controller state updated
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (controller_ && event.cdevice.which == SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(controller_))) {
                SDL_GameControllerClose(controller_);
                controller_ = nullptr;
                return;
            }
        }
    }

    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Get controller type
    SDL_GameControllerType type = SDL_GameControllerGetType(controller_);
    
    // Get button states (combined into bit flags)
    Uint32 buttons = 0;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        if (SDL_GameControllerGetButton(controller_, (SDL_GameControllerButton)i)) {
            buttons |= (1 << i);
        }
    }

    // Get analog inputs
    Sint16 left_x = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 left_y = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY);
    Sint16 right_x = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
    Sint16 right_y = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);
    Sint16 left_trigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    Sint16 right_trigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

    // Get D-pad states
    bool dpad_up = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool dpad_down = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bool dpad_left = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bool dpad_right = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    // Save all states
    csv_file_ << timestamp << ","
              << (int)type << ","
              << buttons << ","
              << left_x << ","
              << left_y << ","
              << right_x << ","
              << right_y << ","
              << left_trigger << ","
              << right_trigger << ","
              << dpad_up << ","
              << dpad_down << ","
              << dpad_left << ","
              << dpad_right << "\n";
}
