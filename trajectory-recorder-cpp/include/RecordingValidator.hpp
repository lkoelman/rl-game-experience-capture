#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "gamepad.pb.h"

namespace trajectory {

struct SyncRow {
    std::uint64_t frame_index{};
    std::uint64_t monotonic_ns{};
    std::uint64_t pts{};
};

enum class ValidationVerdict {
    pass,
    warn,
    fail,
};

struct ValidationConfig {
    double axis_threshold = 0.1;
    std::uint64_t warn_start_dead_ms = 1000;
    std::uint64_t warn_end_dead_ms = 1000;
    std::uint64_t warn_idle_gap_ms = 5000;
    double warn_min_input_rate = 0.1;
    bool strict = false;
};

struct SessionMetrics {
    std::size_t frame_count{};
    std::size_t action_count{};
    std::uint64_t duration_ns{};
    std::uint64_t start_dead_ns{};
    std::uint64_t end_dead_ns{};
    std::uint64_t longest_idle_ns{};
    std::size_t distinct_buttons{};
    std::size_t distinct_axes{};
    std::size_t distinct_keys{};
    std::size_t frames_before_first_input{};
    std::size_t frames_after_last_input{};
    std::size_t stale_frame_count{};
    double average_input_rate{};
    double average_frame_interval_ns{};
    std::uint64_t min_frame_interval_ns{};
    std::uint64_t max_frame_interval_ns{};
};

struct ValidationReport {
    std::filesystem::path session_path;
    std::string session_name;
    SessionMetrics metrics;
    ValidationVerdict verdict{ValidationVerdict::pass};
    std::vector<std::string> warnings;
    std::vector<std::string> failures;
    std::vector<std::pair<std::uint32_t, std::size_t>> button_frequencies;
    std::vector<std::pair<std::uint32_t, std::size_t>> axis_frequencies;
    std::vector<std::pair<std::uint32_t, std::size_t>> key_frequencies;
};

struct LoadedSession {
    ValidationReport report;
    std::vector<SyncRow> sync_rows;
    std::vector<GamepadState> actions;
};

struct BatchSummary {
    std::size_t session_count{};
    std::size_t pass_count{};
    std::size_t warn_count{};
    std::size_t fail_count{};
    std::size_t zero_input_sessions{};
    double average_duration_seconds{};
    double average_input_rate{};
};

ValidationReport AnalyzeSession(const std::filesystem::path& session_dir, const ValidationConfig& config);
LoadedSession LoadSession(const std::filesystem::path& session_dir, const ValidationConfig& config);
std::vector<std::filesystem::path> DiscoverSessionDirectories(const std::filesystem::path& target_path);
BatchSummary SummarizeBatch(const std::vector<ValidationReport>& reports);

std::string FormatValidationSummary(const ValidationReport& report, bool warnings_only = false);
std::string BuildBatchJson(const std::vector<ValidationReport>& reports, const BatchSummary& summary);
std::string BuildBatchCsv(const std::vector<ValidationReport>& reports);

const char* ToString(ValidationVerdict verdict);

}  // namespace trajectory
