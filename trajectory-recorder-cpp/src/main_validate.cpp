#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#endif

#include "RecordingValidator.hpp"
#include "ValidateCli.hpp"

namespace {

std::vector<std::string> CollectArguments(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? argc - 1 : 0);
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index] == nullptr ? "" : argv[index]);
    }
    return args;
}

std::string ProgramName(char* argv[]) {
    if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return "validate_recording";
    }
    return argv[0];
}

trajectory::ValidationConfig BuildConfig(const trajectory::validate_cli::Options& options) {
    trajectory::ValidationConfig config;
    config.axis_threshold = options.axis_threshold;
    config.warn_start_dead_ms = options.warn_start_dead_ms;
    config.warn_end_dead_ms = options.warn_end_dead_ms;
    config.warn_idle_gap_ms = options.warn_idle_gap_ms;
    config.warn_min_input_rate = options.warn_min_input_rate;
    config.strict = options.strict;
    return config;
}

void PrintBatchSummary(const trajectory::BatchSummary& summary) {
    std::cout << "Batch summary:\n";
    std::cout << "  sessions: " << summary.session_count << '\n';
    std::cout << "  pass: " << summary.pass_count << '\n';
    std::cout << "  warn: " << summary.warn_count << '\n';
    std::cout << "  fail: " << summary.fail_count << '\n';
    std::cout << "  zero_input_sessions: " << summary.zero_input_sessions << '\n';
    std::cout << "  average_duration_seconds: " << summary.average_duration_seconds << '\n';
    std::cout << "  average_input_rate: " << summary.average_input_rate << '\n';
}

void RunStepMode(const trajectory::LoadedSession& loaded) {
    if (loaded.sync_rows.empty()) {
        std::cout << "No frames available.\n";
        return;
    }

    std::size_t frame_index = 0;
    for (;;) {
        const auto& row = loaded.sync_rows[frame_index];
        const auto elapsed_ns = row.monotonic_ns - loaded.sync_rows.front().monotonic_ns;
        std::cout << "\nFrame " << row.frame_index
                  << " elapsed_ms=" << (elapsed_ns / 1'000'000ULL)
                  << " timestamp_ns=" << row.monotonic_ns << '\n';

        const trajectory::GamepadState* current_action = nullptr;
        for (const auto& action : loaded.actions) {
            if (action.monotonic_ns() <= row.monotonic_ns) {
                current_action = &action;
            } else {
                break;
            }
        }

        if (current_action == nullptr) {
            std::cout << "No input snapshot yet.\n";
        } else {
            std::cout << "Input monotonic_ns=" << current_action->monotonic_ns() << '\n';
            std::cout << "Buttons:";
            for (int index = 0; index < current_action->pressed_buttons_size(); ++index) {
                std::cout << ' ' << current_action->pressed_buttons(index);
            }
            std::cout << "\nAxes:";
            for (int index = 0; index < current_action->axes_size(); ++index) {
                std::cout << ' ' << index << '=' << current_action->axes(index);
            }
            std::cout << "\nKeys:";
            for (int index = 0; index < current_action->pressed_keys_size(); ++index) {
                std::cout << ' ' << current_action->pressed_keys(index);
            }
            std::cout << '\n';
        }

        std::cout << "Controls: left/right arrows, up/down jump 10 frames, q to quit.\n";
#ifdef _WIN32
        const int first = _getch();
        if (first == 'q' || first == 'Q') {
            break;
        }
        if (first != 0 && first != 224) {
            continue;
        }
        const int key = _getch();
        if (key == 77 && frame_index + 1 < loaded.sync_rows.size()) {
            ++frame_index;
        } else if (key == 75 && frame_index > 0) {
            --frame_index;
        } else if (key == 72) {
            frame_index = std::min(frame_index + 10, loaded.sync_rows.size() - 1);
        } else if (key == 80) {
            frame_index = frame_index >= 10 ? frame_index - 10 : 0;
        }
#else
        char key = '\0';
        std::cin >> key;
        if (key == 'q' || key == 'Q') {
            break;
        }
        if (key == 'd' && frame_index + 1 < loaded.sync_rows.size()) {
            ++frame_index;
        } else if (key == 'a' && frame_index > 0) {
            --frame_index;
        } else if (key == 'w') {
            frame_index = std::min(frame_index + 10, loaded.sync_rows.size() - 1);
        } else if (key == 's') {
            frame_index = frame_index >= 10 ? frame_index - 10 : 0;
        }
#endif
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    trajectory::validate_cli::Options options;
    const auto args = CollectArguments(argc, argv);
    if (!trajectory::validate_cli::TryParseArguments(args, ProgramName(argv), options, std::cout, std::cerr)) {
        return 1;
    }

    const auto session_dirs = trajectory::DiscoverSessionDirectories(options.target_path);
    if (session_dirs.empty()) {
        std::cerr << "Error: no session directories found under " << options.target_path << '\n';
        return 1;
    }

    std::vector<std::filesystem::path> filtered_dirs;
    for (const auto& dir : session_dirs) {
        if (!options.session_name.has_value() || dir.filename() == *options.session_name) {
            filtered_dirs.push_back(dir);
        }
    }
    if (filtered_dirs.empty()) {
        std::cerr << "Error: no session matched the requested filter.\n";
        return 1;
    }

    const auto config = BuildConfig(options);

    if (options.mode == trajectory::validate_cli::Mode::step) {
        const auto loaded = trajectory::LoadSession(filtered_dirs.front(), config);
        std::cout << trajectory::FormatValidationSummary(loaded.report, false);
        RunStepMode(loaded);
        return loaded.report.verdict == trajectory::ValidationVerdict::fail ? 1 : 0;
    }

    std::vector<trajectory::ValidationReport> reports;
    reports.reserve(filtered_dirs.size());
    for (const auto& dir : filtered_dirs) {
        reports.push_back(trajectory::AnalyzeSession(dir, config));
    }

    const auto batch_summary = trajectory::SummarizeBatch(reports);
    if (options.json) {
        std::cout << trajectory::BuildBatchJson(reports, batch_summary);
    } else if (options.csv) {
        std::cout << trajectory::BuildBatchCsv(reports);
    } else {
        for (const auto& report : reports) {
            std::cout << trajectory::FormatValidationSummary(report, options.warnings_only) << '\n';
        }
        if (reports.size() > 1) {
            PrintBatchSummary(batch_summary);
        }
    }

    return batch_summary.fail_count > 0 ? 1 : 0;
}
