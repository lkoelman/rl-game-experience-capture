#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ValidateCli.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestDefaultsAreApplied() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"session_a"};

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(ok, "single session path should parse");
    Expect(options.target_path == "session_a", "target path should be preserved");
    Expect(options.mode == trajectory::validate_cli::Mode::summary, "summary mode should be the default");
    Expect(!options.json, "json output should default to false");
    Expect(!options.csv, "csv output should default to false");
    Expect(!options.strict, "strict mode should default to false");
}

void TestStepModeParses() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"session_a", "--mode", "step"};

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(ok, "step mode should parse");
    Expect(options.mode == trajectory::validate_cli::Mode::step, "step mode should be selected");
}

void TestThresholdsParse() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{
        "session_a",
        "--axis-threshold", "0.2",
        "--warn-start-dead-ms", "1200",
        "--warn-end-dead-ms", "900",
        "--warn-idle-gap-ms", "5000",
        "--warn-min-input-rate", "0.75",
        "--strict",
        "--warnings-only",
        "--json",
    };

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(ok, "numeric thresholds should parse");
    Expect(options.axis_threshold == 0.2, "axis threshold should parse");
    Expect(options.warn_start_dead_ms == 1200, "start dead threshold should parse");
    Expect(options.warn_end_dead_ms == 900, "end dead threshold should parse");
    Expect(options.warn_idle_gap_ms == 5000, "idle gap threshold should parse");
    Expect(options.warn_min_input_rate == 0.75, "input rate threshold should parse");
    Expect(options.strict, "strict flag should parse");
    Expect(options.warnings_only, "warnings-only flag should parse");
    Expect(options.json, "json flag should parse");
}

void TestMissingTargetFailsClearly() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args;

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(!ok, "target path should be required");
    Expect(error.str().find("Expected exactly 1 target path") != std::string::npos,
           "error should mention the missing target path");
}

void TestInvalidModeFailsClearly() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"session_a", "--mode", "video"};

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(!ok, "invalid mode should fail");
    Expect(error.str().find("--mode must be one of: summary, step") != std::string::npos,
           "error should describe valid mode values");
}

void TestCsvAndStepModeConflictClearly() {
    trajectory::validate_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"session_a", "--mode", "step", "--csv"};

    const bool ok = trajectory::validate_cli::TryParseArguments(args, "validate_recording", options, output, error);

    Expect(!ok, "csv output should not be allowed in step mode");
    Expect(error.str().find("Step mode does not support --json or --csv") != std::string::npos,
           "error should explain the mode/output conflict");
}

}  // namespace

int main() {
    TestDefaultsAreApplied();
    TestStepModeParses();
    TestThresholdsParse();
    TestMissingTargetFailsClearly();
    TestInvalidModeFailsClearly();
    TestCsvAndStepModeConflictClearly();
    return 0;
}
