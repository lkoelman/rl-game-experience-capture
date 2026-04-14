#include "RecordingValidator.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include "BinaryIO.hpp"

namespace trajectory {

namespace {

struct SessionData {
    std::vector<SyncRow> sync_rows;
    std::vector<GamepadState> actions;
};

std::string EscapeJson(const std::string& value) {
    std::ostringstream escaped;
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped << "\\\\";
            break;
        case '"':
            escaped << "\\\"";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            escaped << ch;
            break;
        }
    }
    return escaped.str();
}

double NsToSeconds(std::uint64_t ns) {
    return static_cast<double>(ns) / 1'000'000'000.0;
}

std::uint64_t MsToNs(std::uint64_t ms) {
    return ms * 1'000'000ULL;
}

void ParseSyncCsv(const std::filesystem::path& path,
                  std::vector<SyncRow>& rows,
                  std::vector<std::string>& failures) {
    std::ifstream input(path);
    if (!input.is_open()) {
        failures.push_back("failed to open sync.csv");
        return;
    }

    std::string line;
    if (!std::getline(input, line)) {
        failures.push_back("sync.csv is empty");
        return;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream stream(line);
        std::string cell;
        SyncRow row{};
        if (!std::getline(stream, cell, ',')) {
            failures.push_back("malformed sync.csv row");
            return;
        }
        row.frame_index = std::stoull(cell);
        if (!std::getline(stream, cell, ',')) {
            failures.push_back("malformed sync.csv row");
            return;
        }
        row.monotonic_ns = std::stoull(cell);
        if (!std::getline(stream, cell, ',')) {
            failures.push_back("malformed sync.csv row");
            return;
        }
        row.pts = std::stoull(cell);
        rows.push_back(row);
    }
}

void ParseActions(const std::filesystem::path& path,
                  std::vector<GamepadState>& actions,
                  std::vector<std::string>& failures) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        failures.push_back("failed to open actions.bin");
        return;
    }

    if (input.peek() == std::char_traits<char>::eof()) {
        return;
    }

    try {
        while (input.peek() != std::char_traits<char>::eof()) {
            const auto payload = ReadLengthPrefixedPayload(input);
            GamepadState state;
            if (!state.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
                failures.push_back("failed to parse gamepad state protobuf");
                return;
            }
            actions.push_back(std::move(state));
        }
    } catch (const std::exception&) {
        failures.push_back("truncated or malformed actions.bin");
    }
}

std::vector<std::pair<std::uint32_t, std::size_t>> SortFrequencyMap(const std::map<std::uint32_t, std::size_t>& counts) {
    std::vector<std::pair<std::uint32_t, std::size_t>> sorted;
    sorted.reserve(counts.size());
    for (const auto& [key, value] : counts) {
        sorted.emplace_back(key, value);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    return sorted;
}

void FinalizeVerdict(ValidationReport& report, bool strict) {
    if (!report.failures.empty()) {
        report.verdict = ValidationVerdict::fail;
        return;
    }
    if (!report.warnings.empty()) {
        report.verdict = strict ? ValidationVerdict::fail : ValidationVerdict::warn;
        return;
    }
    report.verdict = ValidationVerdict::pass;
}

void AnalyzeMetrics(ValidationReport& report, const SessionData& data, const ValidationConfig& config) {
    report.metrics.frame_count = data.sync_rows.size();
    report.metrics.action_count = data.actions.size();

    if (!data.sync_rows.empty()) {
        const auto first_frame_ns = data.sync_rows.front().monotonic_ns;
        const auto last_frame_ns = data.sync_rows.back().monotonic_ns;
        report.metrics.duration_ns = last_frame_ns >= first_frame_ns ? last_frame_ns - first_frame_ns : 0;

        if (data.sync_rows.size() > 1) {
            std::uint64_t sum_intervals = 0;
            report.metrics.min_frame_interval_ns = std::numeric_limits<std::uint64_t>::max();
            for (std::size_t index = 1; index < data.sync_rows.size(); ++index) {
                const auto interval = data.sync_rows[index].monotonic_ns - data.sync_rows[index - 1].monotonic_ns;
                sum_intervals += interval;
                report.metrics.min_frame_interval_ns = std::min(report.metrics.min_frame_interval_ns, interval);
                report.metrics.max_frame_interval_ns = std::max(report.metrics.max_frame_interval_ns, interval);
            }
            report.metrics.average_frame_interval_ns =
                static_cast<double>(sum_intervals) / static_cast<double>(data.sync_rows.size() - 1);
        }
    }

    std::map<std::uint32_t, std::size_t> button_counts;
    std::map<std::uint32_t, std::size_t> axis_counts;
    std::map<std::uint32_t, std::size_t> key_counts;
    std::set<std::uint32_t> distinct_buttons;
    std::set<std::uint32_t> distinct_axes;
    std::set<std::uint32_t> distinct_keys;

    for (const auto& state : data.actions) {
        for (int button_index = 0; button_index < state.pressed_buttons_size(); ++button_index) {
            const auto button = state.pressed_buttons(button_index);
            ++button_counts[button];
            distinct_buttons.insert(button);
        }
        for (int axis_index = 0; axis_index < state.axes_size(); ++axis_index) {
            if (std::fabs(state.axes(axis_index)) >= config.axis_threshold) {
                ++axis_counts[static_cast<std::uint32_t>(axis_index)];
                distinct_axes.insert(static_cast<std::uint32_t>(axis_index));
            }
        }
        for (int key_index = 0; key_index < state.pressed_keys_size(); ++key_index) {
            const auto key = state.pressed_keys(key_index);
            ++key_counts[key];
            distinct_keys.insert(key);
        }
    }

    report.metrics.distinct_buttons = distinct_buttons.size();
    report.metrics.distinct_axes = distinct_axes.size();
    report.metrics.distinct_keys = distinct_keys.size();
    report.button_frequencies = SortFrequencyMap(button_counts);
    report.axis_frequencies = SortFrequencyMap(axis_counts);
    report.key_frequencies = SortFrequencyMap(key_counts);

    if (!data.actions.empty() && !data.sync_rows.empty()) {
        const auto first_action_ns = data.actions.front().monotonic_ns();
        const auto last_action_ns = data.actions.back().monotonic_ns();
        const auto first_frame_ns = data.sync_rows.front().monotonic_ns;
        const auto last_frame_ns = data.sync_rows.back().monotonic_ns;

        report.metrics.start_dead_ns = first_action_ns > first_frame_ns ? first_action_ns - first_frame_ns : 0;
        report.metrics.end_dead_ns = last_frame_ns > last_action_ns ? last_frame_ns - last_action_ns : 0;
        report.metrics.longest_idle_ns = std::max(report.metrics.start_dead_ns, report.metrics.end_dead_ns);
        for (std::size_t index = 1; index < data.actions.size(); ++index) {
            const auto gap = data.actions[index].monotonic_ns() - data.actions[index - 1].monotonic_ns();
            report.metrics.longest_idle_ns = std::max(report.metrics.longest_idle_ns, gap);
        }
    } else if (!data.sync_rows.empty()) {
        report.metrics.longest_idle_ns = report.metrics.duration_ns;
    }

    if (report.metrics.duration_ns > 0) {
        report.metrics.average_input_rate =
            static_cast<double>(report.metrics.action_count) / NsToSeconds(report.metrics.duration_ns);
    }

    std::size_t next_action_index = 0;
    bool fresh_on_frame = false;
    for (const auto& row : data.sync_rows) {
        fresh_on_frame = false;
        while (next_action_index < data.actions.size() &&
               data.actions[next_action_index].monotonic_ns() <= row.monotonic_ns) {
            fresh_on_frame = true;
            ++next_action_index;
        }

        if (data.actions.empty() || row.monotonic_ns < data.actions.front().monotonic_ns()) {
            ++report.metrics.frames_before_first_input;
            ++report.metrics.stale_frame_count;
            continue;
        }
        if (row.monotonic_ns > data.actions.back().monotonic_ns()) {
            ++report.metrics.frames_after_last_input;
            ++report.metrics.stale_frame_count;
            continue;
        }
        if (!fresh_on_frame) {
            ++report.metrics.stale_frame_count;
        }
    }

    if (data.actions.empty()) {
        report.warnings.push_back("session contains zero input snapshots");
    }
    if (report.metrics.start_dead_ns > MsToNs(config.warn_start_dead_ms)) {
        report.warnings.push_back("long dead period at start");
    }
    if (report.metrics.end_dead_ns > MsToNs(config.warn_end_dead_ms)) {
        report.warnings.push_back("long dead period at end");
    }
    if (report.metrics.longest_idle_ns > MsToNs(config.warn_idle_gap_ms)) {
        report.warnings.push_back("long idle gap between input snapshots");
    }
    if (report.metrics.action_count > 0 && report.metrics.average_input_rate < config.warn_min_input_rate) {
        report.warnings.push_back("low average input rate");
    }
}

SessionData LoadSessionData(const std::filesystem::path& session_dir, ValidationReport& report) {
    SessionData data;
    const auto capture_path = session_dir / "capture.mp4";
    const auto sync_path = session_dir / "sync.csv";
    const auto actions_path = session_dir / "actions.bin";

    if (!std::filesystem::exists(capture_path)) {
        report.failures.push_back("missing capture.mp4");
    } else if (!std::filesystem::is_regular_file(capture_path) || std::filesystem::file_size(capture_path) == 0) {
        report.failures.push_back("capture.mp4 is empty or invalid");
    }

    if (!std::filesystem::exists(sync_path)) {
        report.failures.push_back("missing sync.csv");
    } else {
        ParseSyncCsv(sync_path, data.sync_rows, report.failures);
    }

    if (!std::filesystem::exists(actions_path)) {
        report.failures.push_back("missing actions.bin");
    } else {
        ParseActions(actions_path, data.actions, report.failures);
    }

    if (!report.failures.empty()) {
        return data;
    }

    for (std::size_t index = 1; index < data.sync_rows.size(); ++index) {
        if (data.sync_rows[index].frame_index <= data.sync_rows[index - 1].frame_index) {
            report.failures.push_back("sync.csv frame_index values are not strictly increasing");
            break;
        }
        if (data.sync_rows[index].monotonic_ns < data.sync_rows[index - 1].monotonic_ns) {
            report.failures.push_back("sync.csv monotonic_ns values are not monotonic");
            break;
        }
    }
    for (std::size_t index = 1; index < data.actions.size(); ++index) {
        if (data.actions[index].monotonic_ns() < data.actions[index - 1].monotonic_ns()) {
            report.failures.push_back("actions.bin monotonic_ns values are not monotonic");
            break;
        }
    }
    if (data.sync_rows.empty()) {
        report.failures.push_back("sync.csv contains zero frames");
    }

    return data;
}

void AppendFrequencySection(std::ostringstream& out,
                            const char* label,
                            const std::vector<std::pair<std::uint32_t, std::size_t>>& frequencies) {
    out << label << ':';
    if (frequencies.empty()) {
        out << " none\n";
        return;
    }
    out << '\n';
    for (const auto& [value, count] : frequencies) {
        out << "  " << value << ": " << count << '\n';
    }
}

}  // namespace

ValidationReport AnalyzeSession(const std::filesystem::path& session_dir, const ValidationConfig& config) {
    return LoadSession(session_dir, config).report;
}

LoadedSession LoadSession(const std::filesystem::path& session_dir, const ValidationConfig& config) {
    LoadedSession loaded;
    loaded.report.session_path = session_dir;
    loaded.report.session_name = session_dir.filename().string();
    auto data = LoadSessionData(session_dir, loaded.report);
    if (loaded.report.failures.empty()) {
        AnalyzeMetrics(loaded.report, data, config);
    }
    FinalizeVerdict(loaded.report, config.strict);
    loaded.sync_rows = std::move(data.sync_rows);
    loaded.actions = std::move(data.actions);
    return loaded;
}

std::vector<std::filesystem::path> DiscoverSessionDirectories(const std::filesystem::path& target_path) {
    std::vector<std::filesystem::path> sessions;
    if (!std::filesystem::exists(target_path)) {
        return sessions;
    }
    if (std::filesystem::is_directory(target_path) &&
        std::filesystem::exists(target_path / "sync.csv")) {
        sessions.push_back(target_path);
        return sessions;
    }
    if (!std::filesystem::is_directory(target_path)) {
        return sessions;
    }
    for (const auto& entry : std::filesystem::directory_iterator(target_path)) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "sync.csv")) {
            sessions.push_back(entry.path());
        }
    }
    std::sort(sessions.begin(), sessions.end());
    return sessions;
}

BatchSummary SummarizeBatch(const std::vector<ValidationReport>& reports) {
    BatchSummary summary;
    summary.session_count = reports.size();
    double total_duration_seconds = 0.0;
    double total_input_rate = 0.0;
    for (const auto& report : reports) {
        switch (report.verdict) {
        case ValidationVerdict::pass:
            ++summary.pass_count;
            break;
        case ValidationVerdict::warn:
            ++summary.warn_count;
            break;
        case ValidationVerdict::fail:
            ++summary.fail_count;
            break;
        }
        if (report.metrics.action_count == 0) {
            ++summary.zero_input_sessions;
        }
        total_duration_seconds += NsToSeconds(report.metrics.duration_ns);
        total_input_rate += report.metrics.average_input_rate;
    }
    if (!reports.empty()) {
        summary.average_duration_seconds = total_duration_seconds / static_cast<double>(reports.size());
        summary.average_input_rate = total_input_rate / static_cast<double>(reports.size());
    }
    return summary;
}

const char* ToString(ValidationVerdict verdict) {
    switch (verdict) {
    case ValidationVerdict::pass:
        return "PASS";
    case ValidationVerdict::warn:
        return "WARN";
    case ValidationVerdict::fail:
        return "FAIL";
    }
    return "UNKNOWN";
}

std::string FormatValidationSummary(const ValidationReport& report, bool warnings_only) {
    std::ostringstream out;
    out << "Session: " << report.session_name << '\n';
    out << "Verdict: " << ToString(report.verdict) << '\n';
    if (!warnings_only) {
        out << "Frames: " << report.metrics.frame_count << '\n';
        out << "Actions: " << report.metrics.action_count << '\n';
        out << "Duration (s): " << std::fixed << std::setprecision(3) << NsToSeconds(report.metrics.duration_ns) << '\n';
        out << "Average input rate (events/s): " << std::fixed << std::setprecision(3) << report.metrics.average_input_rate << '\n';
        out << "Start dead period (ms): " << report.metrics.start_dead_ns / 1'000'000ULL << '\n';
        out << "End dead period (ms): " << report.metrics.end_dead_ns / 1'000'000ULL << '\n';
        out << "Longest idle gap (ms): " << report.metrics.longest_idle_ns / 1'000'000ULL << '\n';
        out << "Distinct buttons: " << report.metrics.distinct_buttons << '\n';
        out << "Distinct axes: " << report.metrics.distinct_axes << '\n';
        out << "Distinct keys: " << report.metrics.distinct_keys << '\n';
        AppendFrequencySection(out, "Button frequencies", report.button_frequencies);
        AppendFrequencySection(out, "Axis frequencies", report.axis_frequencies);
        AppendFrequencySection(out, "Key frequencies", report.key_frequencies);
    }
    if (!report.failures.empty()) {
        out << "Failures:\n";
        for (const auto& failure : report.failures) {
            out << "  - " << failure << '\n';
        }
    }
    if (!report.warnings.empty()) {
        out << "Warnings:\n";
        for (const auto& warning : report.warnings) {
            out << "  - " << warning << '\n';
        }
    }
    return out.str();
}

std::string BuildBatchJson(const std::vector<ValidationReport>& reports, const BatchSummary& summary) {
    std::ostringstream out;
    out << "{\n  \"sessions\": [\n";
    for (std::size_t index = 0; index < reports.size(); ++index) {
        const auto& report = reports[index];
        out << "    {\n";
        out << "      \"session_name\": \"" << EscapeJson(report.session_name) << "\",\n";
        out << "      \"session_path\": \"" << EscapeJson(report.session_path.string()) << "\",\n";
        out << "      \"verdict\": \"" << ToString(report.verdict) << "\",\n";
        out << "      \"frame_count\": " << report.metrics.frame_count << ",\n";
        out << "      \"action_count\": " << report.metrics.action_count << ",\n";
        out << "      \"duration_seconds\": " << std::fixed << std::setprecision(6) << NsToSeconds(report.metrics.duration_ns) << ",\n";
        out << "      \"average_input_rate\": " << std::fixed << std::setprecision(6) << report.metrics.average_input_rate << ",\n";
        out << "      \"warnings\": [";
        for (std::size_t warning_index = 0; warning_index < report.warnings.size(); ++warning_index) {
            if (warning_index > 0) {
                out << ", ";
            }
            out << "\"" << EscapeJson(report.warnings[warning_index]) << "\"";
        }
        out << "],\n";
        out << "      \"failures\": [";
        for (std::size_t failure_index = 0; failure_index < report.failures.size(); ++failure_index) {
            if (failure_index > 0) {
                out << ", ";
            }
            out << "\"" << EscapeJson(report.failures[failure_index]) << "\"";
        }
        out << "]\n";
        out << "    }";
        if (index + 1 < reports.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ],\n";
    out << "  \"summary\": {\n";
    out << "    \"session_count\": " << summary.session_count << ",\n";
    out << "    \"pass_count\": " << summary.pass_count << ",\n";
    out << "    \"warn_count\": " << summary.warn_count << ",\n";
    out << "    \"fail_count\": " << summary.fail_count << ",\n";
    out << "    \"zero_input_sessions\": " << summary.zero_input_sessions << ",\n";
    out << "    \"average_duration_seconds\": " << std::fixed << std::setprecision(6) << summary.average_duration_seconds << ",\n";
    out << "    \"average_input_rate\": " << std::fixed << std::setprecision(6) << summary.average_input_rate << '\n';
    out << "  }\n}\n";
    return out.str();
}

std::string BuildBatchCsv(const std::vector<ValidationReport>& reports) {
    std::ostringstream out;
    out << "session_name,verdict,duration_seconds,frame_count,action_count,average_input_rate,start_dead_ms,end_dead_ms,longest_idle_ms,warning_count,failure_count\n";
    for (const auto& report : reports) {
        out << report.session_name << ','
            << ToString(report.verdict) << ','
            << std::fixed << std::setprecision(6) << NsToSeconds(report.metrics.duration_ns) << ','
            << report.metrics.frame_count << ','
            << report.metrics.action_count << ','
            << std::fixed << std::setprecision(6) << report.metrics.average_input_rate << ','
            << report.metrics.start_dead_ns / 1'000'000ULL << ','
            << report.metrics.end_dead_ns / 1'000'000ULL << ','
            << report.metrics.longest_idle_ns / 1'000'000ULL << ','
            << report.warnings.size() << ','
            << report.failures.size() << '\n';
    }
    return out.str();
}

}  // namespace trajectory
