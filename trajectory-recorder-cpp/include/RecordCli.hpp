#pragma once

#include <cctype>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace trajectory::record_cli {

enum class CaptureMode {
    interactive,
    monitor,
    window,
};

struct Options {
    std::string output_dir = "./data";
    std::string session_name = "cpp_session";
    CaptureMode capture_mode = CaptureMode::interactive;
    bool verbose = false;
    int monitor_id = 0;
    std::string window_title;
};

enum class RunStage {
    argument_validation,
    capture_selection,
    signal_handler_installation,
    gstreamer_initialization,
    session_construction,
    session_start,
    wait_for_shutdown,
    session_stop,
};

inline bool HasNonWhitespace(std::string_view value) {
    for (const unsigned char ch : value) {
        if (!std::isspace(ch)) {
            return true;
        }
    }
    return false;
}

inline std::string BuildUsage(std::string_view program_name) {
    return "Usage: " + std::string(program_name) +
           " [output_dir] [session_name] [--monitor <id> | --window <title>] [--verbose]";
}

inline const char* DescribeStage(RunStage stage) {
    switch (stage) {
    case RunStage::argument_validation:
        return "validating command-line arguments";
    case RunStage::capture_selection:
        return "selecting the capture target";
    case RunStage::signal_handler_installation:
        return "installing shutdown handlers";
    case RunStage::gstreamer_initialization:
        return "initializing GStreamer";
    case RunStage::session_construction:
        return "preparing the output session";
    case RunStage::session_start:
        return "starting the recording session";
    case RunStage::wait_for_shutdown:
        return "waiting for a shutdown signal";
    case RunStage::session_stop:
        return "stopping the recording session";
    }

    return "running the recorder";
}

inline bool TryParseArguments(const std::vector<std::string>& args,
                              std::string_view program_name,
                              Options& options,
                              std::ostream& output,
                              std::ostream& error) {
    static_cast<void>(output);

    std::vector<std::string> positional_args;
    std::optional<int> monitor_id;
    std::optional<std::string> window_title;

    auto parse_monitor_id = [](std::string_view value, int& parsed_value) {
        if (value.empty()) {
            return false;
        }

        std::string value_string(value);
        std::istringstream stream(value_string);
        int parsed = 0;
        char trailing = '\0';
        if (!(stream >> parsed) || (stream >> trailing) || parsed <= 0) {
            return false;
        }

        parsed_value = parsed;
        return true;
    };

    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "--verbose") {
            options.verbose = true;
            continue;
        }

        if (args[index] == "--monitor") {
            if (monitor_id.has_value() || window_title.has_value()) {
                error << "Error: Specify only one of --monitor or --window.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            if (index + 1 >= args.size()) {
                error << "Error: --monitor requires an id value.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }

            int parsed_monitor_id = 0;
            if (!parse_monitor_id(args[index + 1], parsed_monitor_id)) {
                error << "Error: monitor id must be a positive integer.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }

            monitor_id = parsed_monitor_id;
            ++index;
            continue;
        }

        if (args[index] == "--window") {
            if (monitor_id.has_value() || window_title.has_value()) {
                error << "Error: Specify only one of --monitor or --window.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            if (index + 1 >= args.size() || !HasNonWhitespace(args[index + 1])) {
                error << "Error: --window requires a non-empty title.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }

            window_title = args[index + 1];
            ++index;
            continue;
        }

        positional_args.push_back(args[index]);
    }

    if (positional_args.size() > 2) {
        error << "Error: Expected at most 2 arguments but received " << positional_args.size() << ".\n"
              << BuildUsage(program_name) << '\n';
        return false;
    }

    if (!positional_args.empty()) {
        if (!HasNonWhitespace(positional_args[0])) {
            error << "Error: output_dir must not be empty.\n"
                  << BuildUsage(program_name) << '\n';
            return false;
        }
        options.output_dir = positional_args[0];
    }

    if (positional_args.size() == 2) {
        if (!HasNonWhitespace(positional_args[1])) {
            error << "Error: session_name must not be empty.\n"
                  << BuildUsage(program_name) << '\n';
            return false;
        }
        options.session_name = positional_args[1];
    }

    if (monitor_id.has_value()) {
        options.capture_mode = CaptureMode::monitor;
        options.monitor_id = *monitor_id;
        options.window_title.clear();
    } else if (window_title.has_value()) {
        options.capture_mode = CaptureMode::window;
        options.monitor_id = 0;
        options.window_title = *window_title;
    } else {
        options.capture_mode = CaptureMode::interactive;
        options.monitor_id = 0;
        options.window_title.clear();
    }

    return true;
}

}  // namespace trajectory::record_cli
