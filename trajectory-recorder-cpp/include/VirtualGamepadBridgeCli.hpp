#pragma once

#include <cstdlib>
#include <iosfwd>
#include <ostream>
#include <string_view>
#include <string>
#include <vector>

namespace trajectory::virtual_gamepad_bridge_cli {

struct Options {
    int max_forward_log_lines_per_second{30};
};

std::string BuildUsage(const std::string& program_name);
bool TryParseArguments(const std::vector<std::string>& args,
                       const std::string& program_name,
                       Options& options,
                       std::ostream& output,
                       std::ostream& error);

inline std::string BuildUsage(const std::string& program_name) {
    return program_name + " [--max-forward-log-lines-per-second <n>]";
}

inline bool TryParsePositiveInt(std::string_view value, int& parsed) {
    if (value.empty()) {
        return false;
    }

    char* end = nullptr;
    const long parsed_long = std::strtol(std::string(value).c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed_long <= 0 || parsed_long > 1'000'000L) {
        return false;
    }

    parsed = static_cast<int>(parsed_long);
    return true;
}

inline bool TryParseArguments(const std::vector<std::string>& args,
                              const std::string& program_name,
                              Options& options,
                              std::ostream& output,
                              std::ostream& error) {
    (void)output;

    options = Options{};

    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "--max-forward-log-lines-per-second") {
            if (index + 1 >= args.size() || !TryParsePositiveInt(args[index + 1], options.max_forward_log_lines_per_second)) {
                error << "Error: --max-forward-log-lines-per-second requires a positive integer.\n"
                      << "Usage: " << BuildUsage(program_name) << '\n';
                return false;
            }

            ++index;
            continue;
        }

        error << "Error: Unexpected argument: " << args[index] << '\n'
              << "Usage: " << BuildUsage(program_name) << '\n';
        return false;
    }

    return true;
}

}  // namespace trajectory::virtual_gamepad_bridge_cli
