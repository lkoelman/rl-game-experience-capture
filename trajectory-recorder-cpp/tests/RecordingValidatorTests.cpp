#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "BinaryIO.hpp"
#include "RecordingValidator.hpp"
#include "TestWindowsSetup.hpp"
#include "gamepad.pb.h"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path MakeTempDir(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("trajectory-validator-" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void WriteSyncCsv(const std::filesystem::path& path, const std::vector<std::uint64_t>& timestamps) {
    std::ofstream out(path);
    out << "frame_index,monotonic_ns,pts\n";
    for (std::size_t index = 0; index < timestamps.size(); ++index) {
        out << index << ',' << timestamps[index] << ',' << (timestamps[index] - timestamps.front()) << '\n';
    }
}

void WriteActions(const std::filesystem::path& path, const std::vector<trajectory::GamepadState>& states) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    for (const auto& state : states) {
        std::string payload;
        if (!state.SerializeToString(&payload)) {
            throw std::runtime_error("failed to serialize test state");
        }
        trajectory::WriteLengthPrefixedPayload(out, payload);
    }
}

trajectory::GamepadState MakeState(std::uint64_t monotonic_ns,
                                   std::initializer_list<float> axes,
                                   std::initializer_list<std::uint32_t> buttons,
                                   std::initializer_list<std::uint32_t> keys = {}) {
    trajectory::GamepadState state;
    state.set_monotonic_ns(monotonic_ns);
    for (const float axis : axes) {
        state.add_axes(axis);
    }
    for (const auto button : buttons) {
        state.add_pressed_buttons(button);
    }
    for (const auto key : keys) {
        state.add_pressed_keys(key);
    }
    return state;
}

void TestAnalyzeSessionComputesSummaryMetrics() {
    const auto root = MakeTempDir("summary");
    const auto session_dir = root / "session_a";
    std::filesystem::create_directories(session_dir);

    WriteSyncCsv(session_dir / "sync.csv", {1'000'000'000ULL, 2'000'000'000ULL, 3'000'000'000ULL, 4'000'000'000ULL});
    WriteActions(session_dir / "actions.bin",
                 {
                     MakeState(1'500'000'000ULL, {0.0f, 0.6f}, {1}, {4}),
                     MakeState(3'500'000'000ULL, {0.0f, 0.0f}, {2}, {}),
                 });
    {
        std::ofstream video(session_dir / "capture.mp4", std::ios::binary | std::ios::trunc);
        video << "not-a-real-video";
    }

    trajectory::ValidationConfig config;
    config.axis_threshold = 0.2;
    config.warn_start_dead_ms = 100;
    config.warn_end_dead_ms = 100;
    config.warn_idle_gap_ms = 1000;
    config.warn_min_input_rate = 1.0;

    const auto report = trajectory::AnalyzeSession(session_dir, config);

    Expect(report.session_name == "session_a", "session name should come from the directory name");
    Expect(report.metrics.frame_count == 4, "frame count should come from sync.csv");
    Expect(report.metrics.action_count == 2, "action count should come from actions.bin");
    Expect(report.metrics.duration_ns == 3'000'000'000ULL, "duration should span first to last frame");
    Expect(report.metrics.start_dead_ns == 500'000'000ULL, "start dead period should be measured from first frame");
    Expect(report.metrics.end_dead_ns == 500'000'000ULL, "end dead period should be measured to last frame");
    Expect(report.metrics.longest_idle_ns == 2'000'000'000ULL, "longest idle gap should be derived from action timestamps");
    Expect(report.metrics.distinct_buttons == 2, "distinct buttons should be counted");
    Expect(report.metrics.distinct_keys == 1, "distinct keys should be counted");
    Expect(report.metrics.distinct_axes == 1, "axes above threshold should be counted");
    Expect(report.metrics.frames_before_first_input == 1, "frames before first input should be counted");
    Expect(report.metrics.frames_after_last_input == 1, "frames after last input should be counted");
    Expect(report.metrics.stale_frame_count == 3, "frames without a fresh snapshot should be counted as stale");
    Expect(report.verdict == trajectory::ValidationVerdict::warn, "low input rate and dead periods should warn");
    Expect(!report.warnings.empty(), "warnings should be recorded when thresholds are exceeded");
}

void TestAnalyzeSessionFlagsMissingFiles() {
    const auto root = MakeTempDir("missing");
    const auto session_dir = root / "session_b";
    std::filesystem::create_directories(session_dir);

    trajectory::ValidationConfig config;
    const auto report = trajectory::AnalyzeSession(session_dir, config);

    Expect(report.verdict == trajectory::ValidationVerdict::fail, "missing required files should fail validation");
    Expect(!report.failures.empty(), "missing file failures should be recorded");
}

void TestAnalyzeSessionFlagsNonMonotonicActions() {
    const auto root = MakeTempDir("non-monotonic");
    const auto session_dir = root / "session_c";
    std::filesystem::create_directories(session_dir);

    WriteSyncCsv(session_dir / "sync.csv", {1'000'000'000ULL, 2'000'000'000ULL, 3'000'000'000ULL});
    WriteActions(session_dir / "actions.bin",
                 {
                     MakeState(2'500'000'000ULL, {0.0f}, {1}),
                     MakeState(2'400'000'000ULL, {0.0f}, {2}),
                 });
    {
        std::ofstream video(session_dir / "capture.mp4", std::ios::binary | std::ios::trunc);
        video << "placeholder";
    }

    trajectory::ValidationConfig config;
    const auto report = trajectory::AnalyzeSession(session_dir, config);

    Expect(report.verdict == trajectory::ValidationVerdict::fail, "non-monotonic action timestamps should fail validation");
    Expect(!report.failures.empty(), "non-monotonic timestamps should be reported as failures");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestAnalyzeSessionComputesSummaryMetrics();
    TestAnalyzeSessionFlagsMissingFiles();
    TestAnalyzeSessionFlagsNonMonotonicActions();
    return 0;
}
