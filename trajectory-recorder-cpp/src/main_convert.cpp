#include <iostream>
#include <stdexcept>
#include <string>

#include "Replayer.hpp"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: convert_dataset <capture.mp4> <sync.csv> <actions.bin>" << std::endl;
        return 1;
    }

    try {
        trajectory::TrajectoryReplayer replayer(argv[1], argv[2], argv[3]);
        cv::Mat frame;
        trajectory::GamepadState action;
        std::size_t count = 0;
        while (replayer.GetNextStep(frame, action)) {
            ++count;
        }
        std::cout << "Aligned " << count << " steps." << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}

