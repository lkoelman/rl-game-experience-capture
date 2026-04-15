#pragma once

#include <cctype>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace trajectory::map_cli {

struct Options {
    std::string game_definition_path;
    std::string output_path = "action-mapping.yaml";
    std::string profile_name = "default";
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
           " <game-actions.yaml> [action-mapping.yaml] [--profile-name <name>]";
}

inline bool TryParseArguments(const std::vector<std::string>& args,
                              std::string_view program_name,
                              Options& options,
                              std::ostream& output,
                              std::ostream& error) {
    static_cast<void>(output);

    std::vector<std::string> positional_args;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "--profile-name") {
            if (index + 1 >= args.size() || !HasNonWhitespace(args[index + 1])) {
                error << "Error: profile name must not be empty.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            options.profile_name = args[index + 1];
            ++index;
            continue;
        }

        positional_args.push_back(args[index]);
    }

    if (positional_args.size() < 1 || positional_args.size() > 2) {
        error << "Error: Expected 1 or 2 positional arguments.\n"
              << BuildUsage(program_name) << '\n';
        return false;
    }

    if (!HasNonWhitespace(positional_args[0])) {
        error << "Error: game definition path must not be empty.\n"
              << BuildUsage(program_name) << '\n';
        return false;
    }
    options.game_definition_path = positional_args[0];

    if (positional_args.size() == 2) {
        if (!HasNonWhitespace(positional_args[1])) {
            error << "Error: output path must not be empty.\n"
                  << BuildUsage(program_name) << '\n';
            return false;
        }
        options.output_path = positional_args[1];
    }

    return true;
}

}  // namespace trajectory::map_cli
