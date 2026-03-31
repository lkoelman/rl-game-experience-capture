#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include <gst/gst.h>

#include "Session.hpp"

std::atomic<bool> g_shutdown_requested{false};

#ifdef _WIN32
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_shutdown_requested = true;
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

void SignalHandler(int) {
    g_shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    std::string output_dir = "./data";
    std::string session_name = "cpp_session";

    if (argc >= 2) {
        output_dir = argv[1];
    }
    if (argc >= 3) {
        session_name = argv[2];
    }

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif

    try {
        gst_init(&argc, &argv);

        trajectory::Session session(output_dir, session_name);
        session.Start();

        std::cout << "Recording... Press Ctrl+C to stop." << std::endl;
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\nGraceful shutdown initiated..." << std::endl;
        session.Stop();
        std::cout << "Session saved successfully." << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}

