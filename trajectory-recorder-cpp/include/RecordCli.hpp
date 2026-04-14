#pragma once

#include <cctype>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace trajectory::record_cli {

struct Options {
    std::string output_dir = "./data";
    std::string session_name = "cpp_session";
};

enum class RunStage {
    argument_validation,
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
    return "Usage: " + std::string(program_name) + " [output_dir] [session_name]";
}

inline const char* DescribeStage(RunStage stage) {
    switch (stage) {
    case RunStage::argument_validation:
        return "validating command-line arguments";
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

    if (args.size() > 2) {
        error << "Error: Expected at most 2 arguments but received " << args.size() << ".\n"
              << BuildUsage(program_name) << '\n';
        return false;
    }

    if (!args.empty()) {
        if (!HasNonWhitespace(args[0])) {
            error << "Error: output_dir must not be empty.\n"
                  << BuildUsage(program_name) << '\n';
            return false;
        }
        options.output_dir = args[0];
    }

    if (args.size() == 2) {
        if (!HasNonWhitespace(args[1])) {
            error << "Error: session_name must not be empty.\n"
                  << BuildUsage(program_name) << '\n';
            return false;
        }
        options.session_name = args[1];
    }

    return true;
}

}  // namespace trajectory::record_cli
