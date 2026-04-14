#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "RecordCli.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestDefaultsAreApplied() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args;

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(ok, "default arguments should parse");
    Expect(options.output_dir == "./data", "default output directory should be used");
    Expect(options.session_name == "cpp_session", "default session name should be used");
    Expect(error.str().empty(), "valid default parse should not write errors");
}

void TestTooManyArgumentsFailsClearly() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"captures", "session_a", "extra"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(!ok, "too many arguments should fail");
    Expect(error.str().find("Expected at most 2 arguments") != std::string::npos, "error should explain the invalid argument count");
    Expect(error.str().find("record_session [output_dir] [session_name]") != std::string::npos, "usage should be printed for invalid arguments");
}

void TestBlankSessionNameFailsClearly() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"captures", ""};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(!ok, "blank session name should fail");
    Expect(error.str().find("session_name must not be empty") != std::string::npos, "error should call out the blank session name");
}

void TestBlankOutputDirectoryFailsClearly() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{""};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(!ok, "blank output directory should fail");
    Expect(error.str().find("output_dir must not be empty") != std::string::npos, "error should call out the blank output directory");
}

void TestFailureMessagesNameTheLifecycleStage() {
    Expect(
        std::string(trajectory::record_cli::DescribeStage(trajectory::record_cli::RunStage::session_start)) == "starting the recording session",
        "session start failures should name the startup stage");
    Expect(
        std::string(trajectory::record_cli::DescribeStage(trajectory::record_cli::RunStage::session_stop)) == "stopping the recording session",
        "session stop failures should name the shutdown stage");
}

void TestMonitorFlagParses() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"captures", "session_a", "--monitor", "2"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(ok, "monitor flag should parse");
    Expect(options.output_dir == "captures", "explicit output directory should be preserved");
    Expect(options.session_name == "session_a", "explicit session name should be preserved");
    Expect(options.capture_mode == trajectory::record_cli::CaptureMode::monitor, "capture mode should be monitor");
    Expect(options.monitor_id == 2, "monitor id should be parsed as one-based");
    Expect(error.str().empty(), "valid monitor flag should not write errors");
}

void TestWindowFlagParses() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--window", "Notepad"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(ok, "window flag should parse");
    Expect(options.capture_mode == trajectory::record_cli::CaptureMode::window, "capture mode should be window");
    Expect(options.window_title == "Notepad", "window title should be preserved");
    Expect(error.str().empty(), "valid window flag should not write errors");
}

void TestVerboseFlagParses() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"captures", "session_a", "--verbose"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(ok, "verbose flag should parse");
    Expect(options.verbose, "verbose flag should enable verbose logging");
    Expect(options.output_dir == "captures", "verbose flag should preserve positional output directory");
    Expect(options.session_name == "session_a", "verbose flag should preserve positional session name");
    Expect(error.str().empty(), "valid verbose flag should not write errors");
}

void TestVerboseFlagCanBeCombinedWithCaptureFlags() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--monitor", "2", "--verbose"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(ok, "verbose flag should coexist with capture flags");
    Expect(options.verbose, "verbose flag should remain enabled when combined with capture flags");
    Expect(options.capture_mode == trajectory::record_cli::CaptureMode::monitor, "monitor flag should still take effect");
    Expect(options.monitor_id == 2, "monitor id should still be parsed");
    Expect(error.str().empty(), "valid verbose + monitor parse should not write errors");
}

void TestConflictingCaptureFlagsFailClearly() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--monitor", "1", "--window", "Notepad"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(!ok, "conflicting capture flags should fail");
    Expect(error.str().find("Specify only one of --monitor or --window") != std::string::npos,
           "error should explain the conflicting capture flags");
}

void TestInvalidMonitorIdFailsClearly() {
    trajectory::record_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--monitor", "0"};

    const bool ok = trajectory::record_cli::TryParseArguments(args, "record_session", options, output, error);

    Expect(!ok, "zero monitor id should fail");
    Expect(error.str().find("monitor id must be a positive integer") != std::string::npos,
           "error should explain the invalid monitor id");
}

void TestUsageIncludesCaptureFlags() {
    const std::string usage = trajectory::record_cli::BuildUsage("record_session");

    Expect(usage.find("[--monitor <id> | --window <title>]") != std::string::npos,
           "usage should describe the capture selection flags");
    Expect(usage.find("[--verbose]") != std::string::npos,
           "usage should describe the verbose flag");
}

}  // namespace

int main() {
    TestDefaultsAreApplied();
    TestTooManyArgumentsFailsClearly();
    TestBlankSessionNameFailsClearly();
    TestBlankOutputDirectoryFailsClearly();
    TestFailureMessagesNameTheLifecycleStage();
    TestMonitorFlagParses();
    TestWindowFlagParses();
    TestVerboseFlagParses();
    TestVerboseFlagCanBeCombinedWithCaptureFlags();
    TestConflictingCaptureFlagsFailClearly();
    TestInvalidMonitorIdFailsClearly();
    TestUsageIncludesCaptureFlags();
    return 0;
}
