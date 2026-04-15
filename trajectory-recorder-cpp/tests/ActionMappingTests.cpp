#include <stdexcept>
#include <string>
#include <vector>

#include "ActionMapping.hpp"

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

    SpecializationDefinition spec;
    spec.id = "fire";
    spec.label = "Fire";
    spec.actions.push_back(ActionDefinition{"cast_fireball", "Cast Fireball", "", ActionInputKind::digital, true});
    klass.specializations.push_back(spec);

    game.classes.push_back(klass);
    return game;
}

void TestCollectActionsIncludesClassAndSpecActions() {
    const auto game = BuildGameDefinition();
    const auto actions = trajectory::mapping::CollectActions(game, "mage", "fire");

    Expect(actions.size() == 3, "class and specialization actions should be combined");
    Expect(actions[0].id == "jump", "class actions should remain available");
    Expect(actions[2].id == "cast_fireball", "specialization actions should be appended");
}

void TestValidationFindsDuplicateBindingsAcrossActions() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.spec_id = "fire";
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
    profile.spec_id = "fire";
    profile.profile_name = "default";
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    Expect(!validation.ok, "missing required actions should fail validation");
    Expect(validation.issues.size() == 2, "two required actions should remain unmapped");
}

void TestWorkflowSupportsSkipConfirmAndEdit() {
    const auto game = BuildGameDefinition();
    trajectory::mapping::MappingWorkflowState workflow(trajectory::mapping::CollectActions(game, "mage", "fire"));

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

}  // namespace

int main() {
    TestCollectActionsIncludesClassAndSpecActions();
    TestValidationFindsDuplicateBindingsAcrossActions();
    TestValidationFindsMissingRequiredActions();
    TestWorkflowSupportsSkipConfirmAndEdit();
    return 0;
}
