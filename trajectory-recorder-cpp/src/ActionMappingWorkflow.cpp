#include "ActionMappingWorkflow.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "GamepadBindingCapture.hpp"

namespace trajectory::mapping {

namespace {

// Formats a UTC timestamp for profile metadata written during save.
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

// Presents a simple selectable list in the terminal and returns the chosen index.
std::optional<std::size_t> PromptForSelection(const std::string& title, const std::vector<std::string>& labels) {
    if (labels.empty()) {
        return std::nullopt;
    }

    int selected = 0;
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
                   text("Use arrow keys and Enter. Press q or Esc to cancel."),
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

// Runs the per-action capture screen until the operator confirms, skips, or cancels.
bool RunActionCaptureScreen(MappingWorkflowState& workflow, GamepadBindingCapture& capture) {
    bool done = false;
    bool cancelled = false;
    std::string status = "Press Space to capture a binding, Enter to continue, n to skip.";
    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    auto component = ftxui::Renderer([&] {
        using namespace ftxui;
        const auto& action = workflow.CurrentAction();
        const auto& state = workflow.ActionStates()[workflow.CurrentIndex()];

        Elements body;
        body.push_back(text("Map action " + std::to_string(workflow.CurrentIndex() + 1) + " / " + std::to_string(workflow.TotalActions())) | bold);
        body.push_back(separator());
        body.push_back(text("Action: " + action.label));
        body.push_back(text("Action ID: " + action.id));
        body.push_back(text("Input kind: " + std::string(action.kind == ActionInputKind::digital
                                                              ? "digital"
                                                              : (action.kind == ActionInputKind::analog ? "analog" : "trigger"))));
        if (!action.description.empty()) {
            body.push_back(text("Description: " + action.description));
        }
        body.push_back(text("Required: " + std::string(action.required ? "yes" : "no")));
        body.push_back(separator());
        body.push_back(text("Mapped: " + std::to_string(workflow.MappedCount()) +
                            "  Skipped: " + std::to_string(workflow.SkippedCount()) +
                            "  Remaining: " + std::to_string(workflow.RemainingCount())));
        body.push_back(separator());
        if (state.bindings.empty()) {
            body.push_back(text("Current bindings: none"));
        } else {
            body.push_back(text("Current bindings:"));
            for (const auto& binding : state.bindings) {
                body.push_back(text("  " + DescribeBinding(binding)));
            }
        }
        body.push_back(separator());
        body.push_back(text(status));

        return vbox(std::move(body)) | border;
    });

    component = ftxui::CatchEvent(component, [&](ftxui::Event event) {
        if (event == ftxui::Event::Escape || (event.is_character() && event.character() == "q")) {
            cancelled = true;
            done = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event.is_character() && event.character() == "n") {
            workflow.SkipCurrentAction();
            done = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Return) {
            const auto& state = workflow.ActionStates()[workflow.CurrentIndex()];
            if (!state.bindings.empty() || state.skipped) {
                done = true;
                screen.ExitLoopClosure()();
            } else {
                status = "Capture at least one binding or press n to skip.";
            }
            return true;
        }
        if (event.is_character() && event.character() == " ") {
            std::string error;
            const auto observed = capture.WaitForBinding(workflow.CurrentAction().kind, std::chrono::milliseconds(5000), error);
            if (!observed.has_value()) {
                status = error;
            } else {
                workflow.AddBindingToCurrentAction(observed->binding);
                status = "Captured " + observed->label + ". Press Space to add another binding or Enter to continue.";
            }
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return done && !cancelled;
}

// Presents the review screen and returns either save, cancel, or the selected action id.
std::optional<std::string> PromptForReviewAction(const MappingWorkflowState& workflow) {
    std::vector<std::string> labels;
    labels.reserve(workflow.ActionStates().size() + 2);
    labels.push_back("Save and finish");
    labels.push_back("Cancel");
    for (const auto& action : workflow.ActionStates()) {
        const std::string status = action.skipped ? "[skipped]" : (action.bindings.empty() ? "[unmapped]" : "[mapped]");
        labels.push_back(status + " " + action.action_id);
    }

    const auto selection = PromptForSelection("Review action mappings", labels);
    if (!selection.has_value()) {
        return std::nullopt;
    }
    if (*selection == 0) {
        return std::string();
    }
    if (*selection == 1) {
        return std::string("__cancel__");
    }
    return workflow.ActionStates()[*selection - 2].action_id;
}

// Materializes the final mapping profile payload from the completed workflow state.
ActionMappingProfile BuildProfile(const GameDefinition& game,
                                  const std::string& class_id,
                                  const std::string& profile_name,
                                  const MappingWorkflowState& workflow) {
    ActionMappingProfile profile;
    profile.schema_version = 1;
    profile.game_id = game.game_id;
    profile.class_id = class_id;
    profile.profile_name = profile_name;
    profile.created_at = CurrentTimestampUtc();
    profile.updated_at = profile.created_at;
    profile.actions = workflow.BuildProfileActions();
    profile.complete = std::all_of(profile.actions.begin(), profile.actions.end(), [](const ProfileActionMapping& action) {
        return action.skipped || !action.bindings.empty();
    });
    return profile;
}

}  // namespace

std::optional<ActionMappingProfile> RunMappingWorkflow(const GameDefinition& game,
                                                       GamepadBindingCapture& capture,
                                                       const std::string& profile_name) {
    std::vector<std::string> class_labels;
    class_labels.reserve(game.classes.size());
    for (const auto& klass : game.classes) {
        class_labels.push_back(klass.label + " (" + klass.id + ")");
    }
    const auto class_selection = PromptForSelection("Select a class", class_labels);
    if (!class_selection.has_value()) {
        return std::nullopt;
    }

    const ClassDefinition& klass = game.classes[*class_selection];
    MappingWorkflowState workflow(CollectActions(game, klass.id));
    while (!workflow.IsFinished()) {
        if (!RunActionCaptureScreen(workflow, capture)) {
            return std::nullopt;
        }
        workflow.AdvanceAction();
    }

    for (;;) {
        const auto review_selection = PromptForReviewAction(workflow);
        if (!review_selection.has_value()) {
            return std::nullopt;
        }
        if (*review_selection == "__cancel__") {
            return std::nullopt;
        }
        if (review_selection->empty()) {
            return BuildProfile(game, klass.id, profile_name, workflow);
        }
        if (!workflow.SetCurrentActionById(*review_selection)) {
            throw std::runtime_error("failed to select action for review: " + *review_selection);
        }
        workflow.ClearCurrentActionBindings();
        if (!RunActionCaptureScreen(workflow, capture)) {
            return std::nullopt;
        }
        workflow.AdvanceAction();
        while (!workflow.IsFinished()) {
            workflow.AdvanceAction();
        }
    }
}

}  // namespace trajectory::mapping
