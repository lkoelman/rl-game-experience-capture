#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestWindowsSetup.hpp"
#include "VirtualGamepadBridgeCli.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestDefaultsAreApplied() {
    trajectory::virtual_gamepad_bridge_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args;

    const bool ok = trajectory::virtual_gamepad_bridge_cli::TryParseArguments(args, "virtual_gamepad_bridge", options, output, error);

    Expect(ok, "default bridge arguments should parse");
    Expect(options.max_forward_log_lines_per_second == 30, "default log line rate should be 30 per second");
    Expect(error.str().empty(), "default parse should not produce errors");
}

void TestMaxLogRateParses() {
    trajectory::virtual_gamepad_bridge_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--max-forward-log-lines-per-second", "45"};

    const bool ok = trajectory::virtual_gamepad_bridge_cli::TryParseArguments(args, "virtual_gamepad_bridge", options, output, error);

    Expect(ok, "max log rate flag should parse");
    Expect(options.max_forward_log_lines_per_second == 45, "explicit log line rate should be preserved");
}

void TestMissingRateValueFailsClearly() {
    trajectory::virtual_gamepad_bridge_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--max-forward-log-lines-per-second"};

    const bool ok = trajectory::virtual_gamepad_bridge_cli::TryParseArguments(args, "virtual_gamepad_bridge", options, output, error);

    Expect(!ok, "missing log rate value should fail");
    Expect(error.str().find("--max-forward-log-lines-per-second requires a positive integer") != std::string::npos,
           "error should explain the missing rate value");
}

void TestInvalidRateFailsClearly() {
    trajectory::virtual_gamepad_bridge_cli::Options options;
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> args{"--max-forward-log-lines-per-second", "0"};

    const bool ok = trajectory::virtual_gamepad_bridge_cli::TryParseArguments(args, "virtual_gamepad_bridge", options, output, error);

    Expect(!ok, "zero log rate should fail");
    Expect(error.str().find("positive integer") != std::string::npos, "error should require a positive integer");
}

void TestUsageIncludesRateFlag() {
    const std::string usage = trajectory::virtual_gamepad_bridge_cli::BuildUsage("virtual_gamepad_bridge");

    Expect(usage.find("[--max-forward-log-lines-per-second <n>]") != std::string::npos,
           "usage should document the forwarding log rate flag");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestDefaultsAreApplied();
    TestMaxLogRateParses();
    TestMissingRateValueFailsClearly();
    TestInvalidRateFailsClearly();
    TestUsageIncludesRateFlag();
    return 0;
}
