#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "ActionMapping.hpp"
#include "ActionMappingWorkflow.hpp"
#include "ActionMappingYaml.hpp"
#include "GamepadBindingCapture.hpp"
#include "MapCli.hpp"

namespace {

// Copies raw argv values into the string vector expected by the CLI parser.
std::vector<std::string> CollectArguments(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? argc - 1 : 0);
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index] == nullptr ? "" : argv[index]);
    }
    return args;
}

// Returns a stable fallback program name when argv metadata is unavailable.
std::string ProgramName(char* argv[]) {
    if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return "map_actions";
    }
    return argv[0];
}

}  // namespace

int main(int argc, char* argv[]) {
    trajectory::map_cli::Options options;
    if (!trajectory::map_cli::TryParseArguments(CollectArguments(argc, argv), ProgramName(argv), options, std::cout, std::cerr)) {
        return 1;
    }

    try {
        // Load the game-defined action catalog that drives class/spec selection and per-action prompts.
        const auto game = trajectory::mapping::LoadGameDefinition(options.game_definition_path);

        // GamepadBindingCapture owns the SDL lifecycle for the mapper and samples raw controls on demand.
        trajectory::mapping::GamepadBindingCapture capture;
        capture.Start();
        // RunMappingWorkflow coordinates the interactive TUI flow:
        // select class/spec, walk through each action, capture bindings, then review before save.
        const auto maybe_profile = trajectory::mapping::RunMappingWorkflow(game, capture, options.profile_name);
        capture.Stop();
        if (!maybe_profile.has_value()) {
            std::cerr << "Action mapping was cancelled.\n";
            return 1;
        }

        trajectory::mapping::ActionMappingProfile profile = *maybe_profile;

        // ValidateProfile checks that the captured bindings still match the selected catalog and do not conflict.
        const auto validation = trajectory::mapping::ValidateProfile(game, profile);

        // Persist whether the saved profile is complete enough for downstream tooling without follow-up mapping work.
        profile.complete = !std::any_of(validation.issues.begin(), validation.issues.end(), [](const trajectory::mapping::ValidationIssue& issue) {
            return issue.severity == trajectory::mapping::ValidationSeverity::warning;
        });

        // Surface validation findings on stderr so command-line users can see why a profile is incomplete or rejected.
        for (const auto& issue : validation.issues) {
            std::cerr << (issue.severity == trajectory::mapping::ValidationSeverity::error ? "Error" : "Warning")
                      << ": " << issue.message << '\n';
        }

        // Blocking issues mean the profile shape is invalid, so do not write a partially broken mapping file.
        if (trajectory::mapping::HasBlockingIssues(validation)) {
            return 1;
        }

        // SaveActionMappingProfile writes the normalized per-user mapping YAML consumed by downstream dataset tooling.
        trajectory::mapping::SaveActionMappingProfile(profile, options.output_path);
        std::cout << "Saved action mapping profile to " << options.output_path << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
