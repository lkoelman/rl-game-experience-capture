#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace trajectory::validate_cli {

enum class Mode {
    summary,
    step,
};

struct Options {
    std::string target_path;
    Mode mode = Mode::summary;
    bool json = false;
    bool csv = false;
    bool warnings_only = false;
    bool strict = false;
    std::optional<std::string> session_name;
    double axis_threshold = 0.1;
    std::uint64_t warn_start_dead_ms = 1000;
    std::uint64_t warn_end_dead_ms = 1000;
    std::uint64_t warn_idle_gap_ms = 5000;
    double warn_min_input_rate = 0.1;
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
           " <session_dir|sessions_root> [--mode <summary|step>] [--json] [--csv] [--warnings-only] [--session <name>] "
           "[--axis-threshold <float>] [--warn-start-dead-ms <n>] [--warn-end-dead-ms <n>] "
           "[--warn-idle-gap-ms <n>] [--warn-min-input-rate <float>] [--strict]";
}

inline bool TryParseArguments(const std::vector<std::string>& args,
                              std::string_view program_name,
                              Options& options,
                              std::ostream& output,
                              std::ostream& error) {
    static_cast<void>(output);

    auto parse_uint64 = [](std::string_view value, std::uint64_t& parsed_value) {
        if (value.empty()) {
            return false;
        }
        std::string value_string(value);
        std::istringstream stream(value_string);
        std::uint64_t parsed = 0;
        char trailing = '\0';
        if (!(stream >> parsed) || (stream >> trailing)) {
            return false;
        }
        parsed_value = parsed;
        return true;
    };

    auto parse_double = [](std::string_view value, double& parsed_value) {
        if (value.empty()) {
            return false;
        }
        std::string value_string(value);
        std::istringstream stream(value_string);
        double parsed = 0.0;
        char trailing = '\0';
        if (!(stream >> parsed) || (stream >> trailing)) {
            return false;
        }
        parsed_value = parsed;
        return true;
    };

    std::vector<std::string> positional_args;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& arg = args[index];
        if (arg == "--json") {
            options.json = true;
            continue;
        }
        if (arg == "--csv") {
            options.csv = true;
            continue;
        }
        if (arg == "--warnings-only") {
            options.warnings_only = true;
            continue;
        }
        if (arg == "--strict") {
            options.strict = true;
            continue;
        }
        if (arg == "--mode") {
            if (index + 1 >= args.size()) {
                error << "Error: --mode requires a value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            const auto& mode_value = args[++index];
            if (mode_value == "summary") {
                options.mode = Mode::summary;
            } else if (mode_value == "step") {
                options.mode = Mode::step;
            } else {
                error << "Error: --mode must be one of: summary, step.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }
        if (arg == "--session") {
            if (index + 1 >= args.size() || !HasNonWhitespace(args[index + 1])) {
                error << "Error: --session requires a non-empty name.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            options.session_name = args[++index];
            continue;
        }
        if (arg == "--axis-threshold") {
            if (index + 1 >= args.size() || !parse_double(args[++index], options.axis_threshold)) {
                error << "Error: --axis-threshold requires a numeric value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }
        if (arg == "--warn-start-dead-ms") {
            if (index + 1 >= args.size() || !parse_uint64(args[++index], options.warn_start_dead_ms)) {
                error << "Error: --warn-start-dead-ms requires an integer value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }
        if (arg == "--warn-end-dead-ms") {
            if (index + 1 >= args.size() || !parse_uint64(args[++index], options.warn_end_dead_ms)) {
                error << "Error: --warn-end-dead-ms requires an integer value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }
        if (arg == "--warn-idle-gap-ms") {
            if (index + 1 >= args.size() || !parse_uint64(args[++index], options.warn_idle_gap_ms)) {
                error << "Error: --warn-idle-gap-ms requires an integer value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }
        if (arg == "--warn-min-input-rate") {
            if (index + 1 >= args.size() || !parse_double(args[++index], options.warn_min_input_rate)) {
                error << "Error: --warn-min-input-rate requires a numeric value.\n" << BuildUsage(program_name) << '\n';
                return false;
            }
            continue;
        }

        positional_args.push_back(arg);
    }

    if (positional_args.size() != 1 || !HasNonWhitespace(positional_args.front())) {
        error << "Error: Expected exactly 1 target path.\n" << BuildUsage(program_name) << '\n';
        return false;
    }

    options.target_path = positional_args.front();

    if (options.mode == Mode::step && (options.json || options.csv)) {
        error << "Error: Step mode does not support --json or --csv.\n" << BuildUsage(program_name) << '\n';
        return false;
    }

    return true;
}

}  // namespace trajectory::validate_cli
