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

}  // namespace

int main() {
    TestDefaultsAreApplied();
    TestTooManyArgumentsFailsClearly();
    TestBlankSessionNameFailsClearly();
    TestBlankOutputDirectoryFailsClearly();
    TestFailureMessagesNameTheLifecycleStage();
    return 0;
}
