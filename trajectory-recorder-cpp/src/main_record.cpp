#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <gst/gst.h>

#include "CaptureSelector.hpp"
#include "RecordCli.hpp"
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

namespace {

bool InstallSignalHandlers(std::ostream& error) {
    if (std::signal(SIGINT, SignalHandler) == SIG_ERR) {
        error << "Error: Failed to install the SIGINT shutdown handler." << std::endl;
        return false;
    }

    if (std::signal(SIGTERM, SignalHandler) == SIG_ERR) {
        error << "Error: Failed to install the SIGTERM shutdown handler." << std::endl;
        return false;
    }

#ifdef _WIN32
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        error << "Error: Failed to install the Windows console control handler." << std::endl;
        return false;
    }
#endif

    return true;
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
        return "record_session";
    }
    return argv[0];
}

trajectory::CaptureTarget ResolveCaptureTarget(const trajectory::record_cli::Options& options) {
    using trajectory::SelectionResult;
    using trajectory::record_cli::CaptureMode;

    SelectionResult result;
    switch (options.capture_mode) {
    case CaptureMode::interactive:
        result = trajectory::PromptForCaptureTarget(trajectory::EnumerateMonitors(), trajectory::EnumerateWindows());
        break;
    case CaptureMode::monitor:
        result = trajectory::ResolveMonitorTarget(trajectory::EnumerateMonitors(), options.monitor_id);
        break;
    case CaptureMode::window:
        result = trajectory::ResolveWindowTarget(trajectory::EnumerateWindows(), options.window_title);
        break;
    }

    if (!result.ok) {
        throw std::runtime_error(result.error);
    }

    return result.target;
}

void PrintCaptureTarget(const trajectory::CaptureTarget& capture_target) {
    if (capture_target.kind == trajectory::CaptureTargetKind::window) {
        std::cout << "  capture_target: window \"" << capture_target.window_title << "\"" << std::endl;
        return;
    }

    std::cout << "  capture_target: monitor " << capture_target.monitor_id << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
    const auto args = CollectArguments(argc, argv);
    trajectory::record_cli::Options options;
    if (!trajectory::record_cli::TryParseArguments(args, ProgramName(argv), options, std::cout, std::cerr)) {
        return 1;
    }

    std::cout << "Recorder configuration:" << std::endl;
    std::cout << "  output_dir: " << options.output_dir << std::endl;
    std::cout << "  session_name: " << options.session_name << std::endl;
    std::cout << "  verbose: " << (options.verbose ? "true" : "false") << std::endl;

    auto stage = trajectory::record_cli::RunStage::capture_selection;
    trajectory::CaptureTarget capture_target = trajectory::CaptureTarget::PrimaryMonitor();

    try {
        std::cout << "Selecting capture target..." << std::endl;
        capture_target = ResolveCaptureTarget(options);
        PrintCaptureTarget(capture_target);
    } catch (const std::exception& error) {
        std::cerr << "Error while " << trajectory::record_cli::DescribeStage(stage) << ": " << error.what() << std::endl;
        return 1;
    }

    std::cout << "Installing shutdown handlers..." << std::endl;

    if (!InstallSignalHandlers(std::cerr)) {
        return 1;
    }

    stage = trajectory::record_cli::RunStage::signal_handler_installation;
    try {
        // GStreamer must be initialized before Session constructs the recorder pipeline.
        stage = trajectory::record_cli::RunStage::gstreamer_initialization;
        std::cout << "Initializing GStreamer..." << std::endl;
        gst_init(&argc, &argv);

        stage = trajectory::record_cli::RunStage::session_construction;
        std::cout << "Preparing output session..." << std::endl;
        trajectory::Session session(options.output_dir, options.session_name, capture_target, options.verbose);

        stage = trajectory::record_cli::RunStage::session_start;
        std::cout << "Starting recording session..." << std::endl;
        session.Start();

        stage = trajectory::record_cli::RunStage::wait_for_shutdown;
        std::cout << "Recording... Press Ctrl+C to stop." << std::endl;
        while (!g_shutdown_requested) {
            session.PumpEventsOnce();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Session owns the coordinated shutdown order for recorder components.
        std::cout << "\nShutdown requested. Stopping recording session..." << std::endl;
        stage = trajectory::record_cli::RunStage::session_stop;
        session.Stop();
        std::cout << "Session saved successfully." << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "Error while " << trajectory::record_cli::DescribeStage(stage) << ": " << error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error while " << trajectory::record_cli::DescribeStage(stage) << ": unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
