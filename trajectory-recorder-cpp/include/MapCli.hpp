#pragma once

#include <cctype>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace trajectory::map_cli {

// Parsed command-line options for the standalone `map_actions` executable.
struct Options {
    std::string game_definition_path;
    std::string output_path = "action-mapping.yaml";
    std::string profile_name = "default";
    std::string resume_from_path;
    int max_combo_buttons = 2;
};

// Returns true when the input contains at least one non-whitespace character.
inline bool HasNonWhitespace(std::string_view value) {
    for (const unsigned char ch : value) {
        if (!std::isspace(ch)) {
            return true;
        }
    }
    return false;
}

// Builds the mapper CLI usage string used in validation failures.
inline std::string BuildUsage(std::string_view program_name) {
    return "Usage: " + std::string(program_name) +
           " <game-actions.yaml> [action-mapping.yaml] [--profile-name <name>] [--resume-from <existing.yaml>] [--max-combo-buttons <n>]";
}

// Parses mapper CLI arguments, applies defaults, and emits usage on invalid input.
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
        if (args[index] == "--resume-from") {
            if (index + 1 >= args.size() || !HasNonWhitespace(args[index + 1])) {
                error << "Error: resume path must not be empty.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            options.resume_from_path = args[index + 1];
            ++index;
            continue;
        }
        if (args[index] == "--max-combo-buttons") {
            if (index + 1 >= args.size() || !HasNonWhitespace(args[index + 1])) {
                error << "Error: max combo buttons must be a positive integer.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            try {
                options.max_combo_buttons = std::stoi(args[index + 1]);
            } catch (const std::exception&) {
                error << "Error: max combo buttons must be a positive integer.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
            if (options.max_combo_buttons <= 0) {
                error << "Error: max combo buttons must be a positive integer.\n"
                      << BuildUsage(program_name) << '\n';
                return false;
            }
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
