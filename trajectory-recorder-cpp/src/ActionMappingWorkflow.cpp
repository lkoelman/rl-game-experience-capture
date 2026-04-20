#include "ActionMappingWorkflow.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "GamepadBindingCapture.hpp"

namespace trajectory::mapping {

namespace {

std::string CurrentTimestampUtc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};
#ifdef _WIN32
    gmtime_s(&utc_time, &time);
#else
    gmtime_r(&time, &utc_time);
#endif
    std::ostringstream formatted;
    formatted << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return formatted.str();
}

std::string InputKindLabel(ActionInputKind kind) {
    switch (kind) {
    case ActionInputKind::digital:
        return "digital";
    case ActionInputKind::analog:
        return "analog";
    case ActionInputKind::trigger:
        return "trigger";
    }
    return "digital";
}

std::string StatusLabel(MappingActionStatus status) {
    switch (status) {
    case MappingActionStatus::mapped:
        return "mapped";
    case MappingActionStatus::skipped:
        return "skipped";
    case MappingActionStatus::unmapped:
        return "unmapped";
    }
    return "unmapped";
}

ftxui::Element WrappedLine(const std::string& value) {
    return ftxui::paragraph(value);
}

bool BindingsEqual(const ActionBinding& left, const ActionBinding& right) {
    return left.type == right.type &&
           left.control == right.control &&
           left.direction == right.direction &&
           left.threshold == right.threshold &&
           left.combo_components == right.combo_components;
}

std::optional<std::size_t> PromptForSelection(const std::string& title,
                                              const std::vector<std::string>& labels,
                                              int initial_selection = 0,
                                              const std::string& help_text = "Use arrow keys and Enter. Press q or Esc to cancel.") {
    if (labels.empty()) {
        return std::nullopt;
    }

    int selected = std::clamp(initial_selection, 0, static_cast<int>(labels.size() - 1));
    bool accepted = false;
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    auto menu = ftxui::Menu(&labels, &selected);
    auto component = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            accepted = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            accepted = false;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    auto renderer = ftxui::Renderer(component, [&] {
        using namespace ftxui;
        return vbox({
                   text(title) | bold,
                   separator(),
                   WrappedLine(help_text),
                   separator(),
                   component->Render(),
               }) |
               border;
    });

    screen.Loop(renderer);
    if (!accepted) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(selected);
}

enum class StartupChoice {
    start_mapping,
    configure_thresholds,
    cancel,
};

std::optional<StartupChoice> PromptForStartupChoice() {
    const std::vector<std::string> labels{
        "Start action mapping",
        "Configure axis thresholds",
        "Cancel",
    };
    const auto selection = PromptForSelection("Choose mapper mode", labels, 0, "Select the next step before class selection.");
    if (!selection.has_value()) {
        return std::nullopt;
    }
    switch (*selection) {
    case 0:
        return StartupChoice::start_mapping;
    case 1:
        return StartupChoice::configure_thresholds;
    default:
        return StartupChoice::cancel;
    }
}

bool EditAxisThresholds(ActionMappingProfile& profile) {
    profile.axis_button_thresholds = NormalizeAxisButtonThresholds(profile.axis_button_thresholds);
    std::vector<AxisButtonThreshold> working_thresholds = profile.axis_button_thresholds;

    std::vector<std::string> labels;
    labels.reserve(working_thresholds.size());
    for (const auto& threshold : working_thresholds) {
        labels.push_back(threshold.control);
    }

    int selected = 0;
    bool saved = false;
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    auto menu = ftxui::Menu(&labels, &selected);
    auto component = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            saved = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            saved = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::ArrowLeft && !working_thresholds.empty()) {
            auto& threshold = working_thresholds[static_cast<std::size_t>(selected)];
            threshold.threshold = std::max(0.05f, threshold.threshold - 0.05f);
            return true;
        }
        if (event == ftxui::Event::ArrowRight && !working_thresholds.empty()) {
            auto& threshold = working_thresholds[static_cast<std::size_t>(selected)];
            threshold.threshold = std::min(1.0f, threshold.threshold + 0.05f);
            return true;
        }
        return false;
    });

    auto renderer = ftxui::Renderer(component, [&] {
        using namespace ftxui;

        Elements values;
        for (std::size_t index = 0; index < working_thresholds.size(); ++index) {
            const auto& threshold = working_thresholds[index];
            std::ostringstream value;
            value << std::fixed << std::setprecision(2) << threshold.threshold;
            values.push_back(text(threshold.control + ": " + value.str()));
        }

        return vbox({
                   text("Configure axis thresholds") | bold,
                   separator(),
                   WrappedLine("Left/Right adjust the selected axis threshold. Enter saves. q or Esc cancels."),
                   separator(),
                   hbox({
                       component->Render() | flex,
                       separator(),
                       vbox(std::move(values)) | flex,
                   }),
               }) |
               border;
    });

    screen.Loop(renderer);
    if (saved) {
        profile.axis_button_thresholds = NormalizeAxisButtonThresholds(working_thresholds);
    }
    return saved;
}

std::vector<ProfileActionMapping> ExistingActionsForClass(const GameDefinition& game,
                                                          const std::string& class_id,
                                                          const ActionMappingProfile* existing_profile) {
    if (existing_profile == nullptr) {
        return {};
    }
    if (existing_profile->game_id != game.game_id || existing_profile->class_id != class_id) {
        return {};
    }
    return existing_profile->actions;
}

ActionMappingProfile BuildProfile(const GameDefinition& game,
                                  const std::string& class_id,
                                  const std::string& profile_name,
                                  const MappingWorkflowState& workflow,
                                  const ActionMappingProfile& working_profile_seed,
                                  const ActionMappingProfile* existing_profile) {
    const std::string now = CurrentTimestampUtc();

    ActionMappingProfile profile;
    profile.schema_version = existing_profile != nullptr ? existing_profile->schema_version : 1;
    profile.game_id = game.game_id;
    profile.class_id = class_id;
    profile.profile_name = profile_name;
    profile.created_at = existing_profile != nullptr && !existing_profile->created_at.empty()
                             ? existing_profile->created_at
                             : now;
    profile.updated_at = now;
    profile.complete = false;
    profile.axis_button_thresholds = NormalizeAxisButtonThresholds(working_profile_seed.axis_button_thresholds);
    profile.actions = workflow.BuildProfileActions();
    return profile;
}

struct ReviewChoice {
    bool save{false};
    bool cancelled{false};
    std::optional<std::string> edit_action_id;
};

ReviewChoice RunReviewScreen(const GameDefinition& game,
                             const MappingWorkflowState& workflow,
                             const ActionMappingProfile& profile,
                             int max_combo_buttons) {
    const ValidationResult validation = ValidateProfile(game, profile, max_combo_buttons);

    std::vector<std::string> labels;
    labels.reserve(workflow.TotalActions() + 2);
    labels.push_back("Save and finish");
    labels.push_back("Cancel without saving");
    for (std::size_t index = 0; index < workflow.TotalActions(); ++index) {
        const auto& action = workflow.Actions()[index];
        labels.push_back("[" + StatusLabel(workflow.StatusForAction(index)) + "] " + action.label + " (" + action.id + ")");
    }

    int selected = 0;
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    auto menu = ftxui::Menu(&labels, &selected);
    auto component = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            selected = 1;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    auto renderer = ftxui::Renderer(component, [&] {
        using namespace ftxui;

        Elements issues;
        issues.push_back(WrappedLine("Warnings: " + std::to_string(std::count_if(validation.issues.begin(), validation.issues.end(), [](const ValidationIssue& issue) {
            return issue.severity == ValidationSeverity::warning;
        })) +
                              "  Errors: " + std::to_string(std::count_if(validation.issues.begin(), validation.issues.end(), [](const ValidationIssue& issue) {
                                  return issue.severity == ValidationSeverity::error;
                              }))));
        for (const auto& issue : validation.issues) {
            issues.push_back(WrappedLine(std::string(issue.severity == ValidationSeverity::error ? "Error: " : "Warning: ") + issue.message));
        }
        if (validation.issues.empty()) {
            issues.push_back(WrappedLine("No validation issues."));
        }

        return vbox({
                   text("Review action mappings") | bold,
                   separator(),
                   WrappedLine("Enter saves, cancels, or reopens an action for editing."),
                   separator(),
                   vbox(std::move(issues)),
                   separator(),
                   component->Render(),
               }) |
               border;
    });

    screen.Loop(renderer);

    if (selected == 0) {
        return ReviewChoice{true, false, std::nullopt};
    }
    if (selected == 1) {
        return ReviewChoice{false, true, std::nullopt};
    }
    return ReviewChoice{false, false, workflow.Actions()[static_cast<std::size_t>(selected - 2)].id};
}

enum class MappingScreenResult {
    cancelled,
    review,
};

MappingScreenResult RunMappingScreen(MappingWorkflowState& workflow,
                                     GamepadBindingCapture& capture,
                                     const ActionMappingProfile& working_profile_seed,
                                     int max_combo_buttons) {
    if (workflow.IsFinished()) {
        return MappingScreenResult::review;
    }

    std::string status = "Space confirms the last observed binding. c clears current bindings. Right advances or skips. Left goes back. Enter opens review/save.";
    std::optional<ObservedBinding> observed;
    std::vector<std::string> menu_entries;
    int selected = static_cast<int>(workflow.CurrentIndex());
    int right_width = 40;
    bool cancelled = false;

    auto refresh_entries = [&] {
        menu_entries.clear();
        for (std::size_t index = 0; index < workflow.TotalActions(); ++index) {
            const auto& action = workflow.Actions()[index];
            menu_entries.push_back("[" + StatusLabel(workflow.StatusForAction(index)) + "] " + action.label);
        }
        if (!menu_entries.empty()) {
            const std::size_t current = workflow.IsFinished() ? workflow.TotalActions() - 1 : workflow.CurrentIndex();
            selected = static_cast<int>(current);
        }
    };

    refresh_entries();

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    std::atomic<bool> running = true;
    std::thread ticker([&] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            screen.PostEvent(ftxui::Event::Custom);
        }
    });

    auto left = ftxui::Renderer([&] {
        using namespace ftxui;

        if (workflow.IsFinished()) {
            return vbox({
                       text("Mapping complete") | bold,
                       separator(),
                       WrappedLine("All actions have been reviewed. Press Enter to open the review screen."),
                   }) |
                   border;
        }

        const auto& action = workflow.CurrentAction();
        const auto& state = workflow.ActionStates()[workflow.CurrentIndex()];

        Elements bindings;
        if (state.bindings.empty()) {
            bindings.push_back(WrappedLine("Current bindings: none"));
        } else {
            bindings.push_back(WrappedLine("Current bindings:"));
            for (const auto& binding : state.bindings) {
                bindings.push_back(WrappedLine("  " + DescribeBinding(binding)));
            }
        }

        const std::string observed_label = observed.has_value() ? observed->label : "none";

        return vbox({
                   text("Map action " + std::to_string(workflow.CurrentIndex() + 1) + " / " + std::to_string(workflow.TotalActions())) | bold,
                   separator(),
                   WrappedLine("Action: " + action.label),
                   WrappedLine("Action ID: " + action.id),
                   WrappedLine("Input kind: " + InputKindLabel(action.kind)),
                   WrappedLine("Required: " + std::string(action.required ? "yes" : "no")),
                   action.description.empty() ? text("") : WrappedLine("Description: " + action.description),
                   separator(),
                   WrappedLine("Last observed input: " + observed_label),
                   separator(),
                   vbox(std::move(bindings)),
                   separator(),
                   WrappedLine("Mapped: " + std::to_string(workflow.MappedCount()) +
                               "  Skipped: " + std::to_string(workflow.SkippedCount()) +
                               "  Remaining: " + std::to_string(workflow.RemainingCount())),
                   separator(),
                   WrappedLine(status),
               }) |
               border;
    });

    auto menu = ftxui::Menu(&menu_entries, &selected);
    auto layout = ftxui::ResizableSplitLeft(menu, left, &right_width);

    auto component = ftxui::CatchEvent(layout, [&](ftxui::Event event) {
        if (event == ftxui::Event::Custom) {
            if (workflow.IsFinished()) {
                return true;
            }
            observed = capture.PollBinding(workflow.CurrentAction().kind, working_profile_seed, max_combo_buttons);
            if (!capture.CurrentWarning().empty()) {
                status = capture.CurrentWarning();
            }
            refresh_entries();
            return true;
        }
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            cancelled = true;
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
            refresh_entries();
            return true;
        }
        if (event == ftxui::Event::ArrowLeft) {
            workflow.MoveToPreviousAction();
            capture.ClearObservedBindings();
            observed.reset();
            refresh_entries();
            return true;
        }
        if (event.is_character() && event.character() == "c") {
            workflow.ClearCurrentActionBindings();
            capture.ClearObservedBindings();
            observed.reset();
            status = "Cleared mapped bindings and the last observed input for this action.";
            refresh_entries();
            return true;
        }
        if (event == ftxui::Event::ArrowRight) {
            workflow.AdvanceOrSkipCurrentAction();
            capture.ClearObservedBindings();
            observed.reset();
            if (workflow.IsFinished()) {
                running = false;
                screen.ExitLoopClosure()();
            } else {
                refresh_entries();
            }
            return true;
        }
        if (event == ftxui::Event::Return) {
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event.is_character() && event.character() == " ") {
            if (!observed.has_value()) {
                status = "No gamepad input has been observed yet for this action.";
                return true;
            }

            auto& state = workflow.ActionStates()[workflow.CurrentIndex()];
            const bool already_present = std::any_of(state.bindings.begin(), state.bindings.end(), [&](const ActionBinding& binding) {
                return BindingsEqual(binding, observed->binding);
            });
            if (already_present) {
                status = "The last observed binding is already mapped for this action.";
                return true;
            }

            workflow.AddBindingToCurrentAction(observed->binding);
            status = "Captured " + observed->label + ". Press another gamepad input to replace the remembered candidate, or press Right to continue.";
            capture.ClearObservedBindings();
            observed.reset();
            refresh_entries();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    running = false;
    ticker.join();
    capture.ClearObservedBindings();

    if (cancelled) {
        return MappingScreenResult::cancelled;
    }
    return MappingScreenResult::review;
}

}  // namespace

std::optional<ActionMappingProfile> RunMappingWorkflow(const GameDefinition& game,
                                                       GamepadBindingCapture& capture,
                                                       const std::string& profile_name,
                                                       int max_combo_buttons,
                                                       const ActionMappingProfile* existing_profile) {
    ActionMappingProfile working_profile_seed;
    working_profile_seed.schema_version = existing_profile != nullptr ? existing_profile->schema_version : 1;
    working_profile_seed.game_id = game.game_id;
    working_profile_seed.profile_name = profile_name;
    working_profile_seed.axis_button_thresholds = existing_profile != nullptr
                                                     ? NormalizeAxisButtonThresholds(existing_profile->axis_button_thresholds)
                                                     : BuildDefaultAxisButtonThresholds();

    for (;;) {
        const auto startup_choice = PromptForStartupChoice();
        if (!startup_choice.has_value() || *startup_choice == StartupChoice::cancel) {
            return std::nullopt;
        }
        if (*startup_choice == StartupChoice::configure_thresholds) {
            static_cast<void>(EditAxisThresholds(working_profile_seed));
            continue;
        }

        std::vector<std::string> class_labels;
        class_labels.reserve(game.classes.size());
        int initial_selection = 0;
        for (std::size_t index = 0; index < game.classes.size(); ++index) {
            const auto& klass = game.classes[index];
            class_labels.push_back(klass.label + " (" + klass.id + ")");
            if (existing_profile != nullptr && existing_profile->class_id == klass.id) {
                initial_selection = static_cast<int>(index);
            }
        }

        const auto class_selection = PromptForSelection("Select a class", class_labels, initial_selection);
        if (!class_selection.has_value()) {
            return std::nullopt;
        }

        const ClassDefinition& klass = game.classes[*class_selection];
        const ActionMappingProfile* class_existing_profile =
            existing_profile != nullptr && existing_profile->class_id == klass.id ? existing_profile : nullptr;
        MappingWorkflowState workflow(CollectActions(game, klass.id), ExistingActionsForClass(game, klass.id, class_existing_profile));

        for (;;) {
            if (!workflow.IsFinished()) {
                const MappingScreenResult mapping_result = RunMappingScreen(workflow, capture, working_profile_seed, max_combo_buttons);
                if (mapping_result == MappingScreenResult::cancelled) {
                    return std::nullopt;
                }
            }

            ActionMappingProfile profile = BuildProfile(game, klass.id, profile_name, workflow, working_profile_seed, class_existing_profile);
            const ReviewChoice review = RunReviewScreen(game, workflow, profile, max_combo_buttons);
            if (review.cancelled) {
                return std::nullopt;
            }
            if (review.save) {
                return profile;
            }
            if (!review.edit_action_id.has_value()) {
                return std::nullopt;
            }
            if (!workflow.SetCurrentActionById(*review.edit_action_id)) {
                throw std::runtime_error("failed to select action for review: " + *review.edit_action_id);
            }
            workflow.ClearCurrentActionBindings();
            capture.ClearObservedBindings();
        }
    }
}

}  // namespace trajectory::mapping
