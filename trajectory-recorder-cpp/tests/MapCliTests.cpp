#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "MapCli.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestDefaultsAreApplied() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"game-actions.yaml"};

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(ok, "single game definition path should parse");
    Expect(options.game_definition_path == "game-actions.yaml", "game definition path should be preserved");
    Expect(options.output_path == "action-mapping.yaml", "default output path should be applied");
    Expect(options.profile_name == "default", "default profile name should be applied");
}

void TestOptionalArgumentsParse() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{
        "game-actions.yaml",
        "profile.yaml",
        "--profile-name",
        "steam-deck",
    };

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(ok, "output path and profile name should parse");
    Expect(options.output_path == "profile.yaml", "explicit output path should be preserved");
    Expect(options.profile_name == "steam-deck", "profile name should be preserved");
}

void TestResumeFromParsesAlongsideOutputPath() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{
        "game-actions.yaml",
        "profile.yaml",
        "--profile-name",
        "steam-deck",
        "--resume-from",
        "existing.yaml",
    };

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(ok, "resume path should parse");
    Expect(options.output_path == "profile.yaml", "explicit output path should still be preserved");
    Expect(options.resume_from_path == "existing.yaml", "resume path should be preserved");
}

void TestMissingGameDefinitionFailsClearly() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args;

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(!ok, "game definition path should be required");
    Expect(error.str().find("Expected 1 or 2 positional arguments") != std::string::npos,
           "error should explain the required positional arguments");
}

void TestBlankProfileNameFailsClearly() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"game-actions.yaml", "--profile-name", ""};

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(!ok, "blank profile name should fail");
    Expect(error.str().find("profile name must not be empty") != std::string::npos,
           "error should explain the invalid profile name");
}

void TestMissingResumePathFailsClearly() {
    trajectory::map_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"game-actions.yaml", "--resume-from"};

    const bool ok = trajectory::map_cli::TryParseArguments(args, "map_actions", options, output, error);

    Expect(!ok, "missing resume path should fail");
    Expect(error.str().find("resume path must not be empty") != std::string::npos,
           "error should explain the invalid resume path");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestDefaultsAreApplied();
    TestOptionalArgumentsParse();
    TestResumeFromParsesAlongsideOutputPath();
    TestMissingGameDefinitionFailsClearly();
    TestBlankProfileNameFailsClearly();
    TestMissingResumePathFailsClearly();
    return 0;
}
