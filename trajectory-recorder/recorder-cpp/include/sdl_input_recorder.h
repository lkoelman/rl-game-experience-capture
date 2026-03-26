#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <fstream>

/**
 * @brief Records gamepad input using SDL's GameController API
 * 
 * This class provides functionality to record gamepad state using SDL's
 * cross-platform GameController API. It saves the data in CSV format with
 * timestamps and all controller inputs.
 */
class SdlInputRecorder {
public:
    /**
     * @brief Construct a new SDL Input Recorder
     */
    SdlInputRecorder();
    
    /**
     * @brief Destroy the SDL Input Recorder and cleanup resources
     */
    ~SdlInputRecorder();

    /**
     * @brief Start recording gamepad input to a file
     * 
     * @param output_file Path to the output CSV file
     * @return true if recording started successfully
     * @return false if there was an error or already recording
     */
    bool StartRecording(const std::string& output_file);
    
    /**
     * @brief Stop the current recording session
     */
    void StopRecording();
    
    /**
     * @brief Update the recorder state and save current gamepad values
     * 
     * Should be called regularly (e.g. in main loop) to capture input
     */
    void Update();

private:
    bool recording_;
    std::ofstream csv_file_;
    std::string output_file_;
    SDL_GameController* controller_;
};
