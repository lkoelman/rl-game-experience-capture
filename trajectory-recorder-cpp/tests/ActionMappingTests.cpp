#include <stdexcept>
#include <string>
#include <vector>

#include "ActionMapping.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

trajectory::mapping::GameDefinition BuildGameDefinition() {
    using namespace trajectory::mapping;

    GameDefinition game;
    game.game_id = "demo";
    game.display_name = "Demo Game";

    ClassDefinition klass;
    klass.id = "mage";
    klass.label = "Mage";
    klass.actions.push_back(ActionDefinition{"jump", "Jump", "", ActionInputKind::digital, true});
    klass.actions.push_back(ActionDefinition{"move_x", "Move X", "", ActionInputKind::analog, true});
    klass.actions.push_back(ActionDefinition{"cast_fireball", "Cast Fireball", "", ActionInputKind::digital, true});

    game.classes.push_back(klass);
    return game;
}

void TestCollectActionsReturnsClassActions() {
    const auto game = BuildGameDefinition();
    const auto actions = trajectory::mapping::CollectActions(game, "mage");

    Expect(actions.size() == 3, "class actions should be returned in order");
    Expect(actions[0].id == "jump", "class actions should remain available");
    Expect(actions[2].id == "cast_fireball", "later class actions should remain available");
}

void TestValidationFindsDuplicateBindingsAcrossActions() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Button("south")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    Expect(!validation.ok, "duplicate bindings should fail validation");
    bool found_duplicate = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("south") != std::string::npos) {
            found_duplicate = true;
            break;
        }
    }
    Expect(found_duplicate, "duplicate binding message should name the conflicting control");
}

void TestValidationFindsMissingRequiredActions() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    Expect(!validation.ok, "missing required actions should fail validation");
    Expect(validation.issues.size() == 2, "two required actions should remain unmapped");
}

void TestWorkflowSupportsSkipConfirmAndEdit() {
    const auto game = BuildGameDefinition();
    trajectory::mapping::MappingWorkflowState workflow(trajectory::mapping::CollectActions(game, "mage"));

    workflow.AddBindingToCurrentAction(trajectory::mapping::ActionBinding::Button("south"));
    workflow.AdvanceAction();
    workflow.SkipCurrentAction();
    workflow.AdvanceAction();
    workflow.AddBindingToCurrentAction(trajectory::mapping::ActionBinding::Button("east"));
    workflow.SetCurrentActionById("move_x");
    workflow.ClearCurrentActionBindings();
    workflow.AddBindingToCurrentAction(trajectory::mapping::ActionBinding::Axis("leftx", "any"));

    const auto profile_actions = workflow.BuildProfileActions();

    Expect(profile_actions.size() == 3, "workflow should produce one profile action per applicable action");
    Expect(!profile_actions[1].skipped, "editing a skipped action should clear the skipped state");
    Expect(profile_actions[1].bindings[0].control == "leftx", "edited action should retain the replacement binding");
    Expect(profile_actions[2].bindings[0].control == "east", "other mapped actions should remain unchanged");
}

void TestWorkflowPreloadsExistingMappingsAndStartsAtFirstUnresolvedAction() {
    const auto game = BuildGameDefinition();
    std::vector<trajectory::mapping::ProfileActionMapping> existing_actions;
    existing_actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});
    existing_actions.push_back({"move_x", true, {}});

    trajectory::mapping::MappingWorkflowState workflow(trajectory::mapping::CollectActions(game, "mage"), existing_actions);

    Expect(workflow.CurrentIndex() == 2, "workflow should start at the first unresolved action");
    const auto profile_actions = workflow.BuildProfileActions();
    Expect(profile_actions[0].bindings[0].control == "south", "existing bindings should be preserved");
    Expect(profile_actions[1].skipped, "existing skipped actions should be preserved");
}

void TestWorkflowNavigationSupportsPreviousAndRightArrowSkipBehavior() {
    const auto game = BuildGameDefinition();
    trajectory::mapping::MappingWorkflowState workflow(trajectory::mapping::CollectActions(game, "mage"));

    workflow.AdvanceOrSkipCurrentAction();
    Expect(workflow.ActionStates()[0].skipped, "advancing without a binding should skip the current action");
    Expect(workflow.CurrentIndex() == 1, "advancing should move to the next action");

    workflow.ReplaceCurrentActionBindings({trajectory::mapping::ActionBinding::Axis("leftx", "any")});
    workflow.AdvanceOrSkipCurrentAction();
    Expect(workflow.CurrentIndex() == 2, "advancing with a binding should preserve progress");

    workflow.MoveToPreviousAction();
    Expect(workflow.CurrentIndex() == 1, "moving left should revisit the previous action");
    Expect(!workflow.ActionStates()[1].skipped, "moving left should not mutate the previous action");
    Expect(workflow.ActionStates()[1].bindings[0].control == "leftx", "existing bindings should remain intact when navigating");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestCollectActionsReturnsClassActions();
    TestValidationFindsDuplicateBindingsAcrossActions();
    TestValidationFindsMissingRequiredActions();
    TestWorkflowSupportsSkipConfirmAndEdit();
    TestWorkflowPreloadsExistingMappingsAndStartsAtFirstUnresolvedAction();
    TestWorkflowNavigationSupportsPreviousAndRightArrowSkipBehavior();
    return 0;
}
