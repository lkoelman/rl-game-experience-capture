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
    klass.actions.push_back(ActionDefinition{"move_character", "Move Character", "", ActionInputKind::vector2, true});
    klass.actions.push_back(ActionDefinition{"cast_fireball", "Cast Fireball", "", ActionInputKind::digital, true});

    game.classes.push_back(klass);
    return game;
}

void TestCollectActionsReturnsClassActions() {
    const auto game = BuildGameDefinition();
    const auto actions = trajectory::mapping::CollectActions(game, "mage");

    Expect(actions.size() == 4, "class actions should be returned in order");
    Expect(actions[0].id == "jump", "class actions should remain available");
    Expect(actions[3].id == "cast_fireball", "later class actions should remain available");
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
    Expect(validation.issues.size() == 3, "three required actions should remain unmapped");
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

    Expect(profile_actions.size() == 4, "workflow should produce one profile action per applicable action");
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

void TestValidationAcceptsStickBindingForVector2Action() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});
    profile.actions.push_back({"move_x", false, {trajectory::mapping::ActionBinding::Axis("leftx", "any")}});
    profile.actions.push_back({"move_character", false, {trajectory::mapping::ActionBinding::Stick("left_stick")}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Button("east")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    bool found_error = false;
    for (const auto& issue : validation.issues) {
        if (issue.severity == trajectory::mapping::ValidationSeverity::error) {
            found_error = true;
            break;
        }
    }
    Expect(!found_error, "vector2 actions should accept stick bindings");
}

void TestValidationRejectsAxisBindingForVector2Action() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"move_character", false, {trajectory::mapping::ActionBinding::Axis("leftx", "any")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    bool found_kind_error = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("vector2") != std::string::npos) {
            found_kind_error = true;
            break;
        }
    }
    Expect(found_kind_error, "vector2 actions should reject scalar axis bindings");
}

void TestValidationRejectsStickBindingForAnalogAction() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"move_x", false, {trajectory::mapping::ActionBinding::Stick("left_stick")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    bool found_kind_error = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("analog") != std::string::npos && issue.message.find("stick") != std::string::npos) {
            found_kind_error = true;
            break;
        }
    }
    Expect(found_kind_error, "analog actions should reject stick bindings");
}

void TestValidationFindsDuplicateStickBindingsAcrossActions() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.actions.push_back({"move_character", false, {trajectory::mapping::ActionBinding::Stick("left_stick")}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Stick("left_stick")}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    bool found_duplicate = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("left_stick") != std::string::npos) {
            found_duplicate = true;
            break;
        }
    }
    Expect(found_duplicate, "duplicate stick bindings should fail validation");
}

void TestDescribeBindingFormatsStickBinding() {
    const auto description = trajectory::mapping::DescribeBinding(
        trajectory::mapping::ActionBinding::Stick("right_stick"));

    Expect(description == "stick:right_stick", "stick bindings should have a stable description");
}

void TestValidationFindsDuplicateComboBindingsAcrossActions() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.axis_button_thresholds = trajectory::mapping::BuildDefaultAxisButtonThresholds();
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Combo({
                                              trajectory::mapping::ComboComponent::Button("south"),
                                              trajectory::mapping::ComboComponent::Button("left_shoulder"),
                                          })}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Combo({
                                                        trajectory::mapping::ComboComponent::Button("left_shoulder"),
                                                        trajectory::mapping::ComboComponent::Button("south"),
                                                    })}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    Expect(!validation.ok, "duplicate combo bindings should fail validation");
    bool found_duplicate = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("left_shoulder") != std::string::npos && issue.message.find("south") != std::string::npos) {
            found_duplicate = true;
            break;
        }
    }
    Expect(found_duplicate, "duplicate combo message should name the conflicting controls");
}

void TestValidationRejectsOversizedAndDuplicateComboMembers() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.axis_button_thresholds = trajectory::mapping::BuildDefaultAxisButtonThresholds();
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Combo({
                                              trajectory::mapping::ComboComponent::Button("south"),
                                              trajectory::mapping::ComboComponent::Button("east"),
                                          })}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Combo({
                                                        trajectory::mapping::ComboComponent::Button("south"),
                                                        trajectory::mapping::ComboComponent::Button("south"),
                                                    })}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile, 1);

    Expect(!validation.ok, "oversized or duplicate combo members should fail validation");
    bool found_size_issue = false;
    bool found_duplicate_member = false;
    for (const auto& issue : validation.issues) {
        if (issue.message.find("maximum") != std::string::npos) {
            found_size_issue = true;
        }
        if (issue.message.find("duplicate combo member") != std::string::npos) {
            found_duplicate_member = true;
        }
    }
    Expect(found_size_issue, "oversized combo should report the configured maximum");
    Expect(found_duplicate_member, "duplicate combo members should be rejected");
}

void TestValidationAcceptsDirectionalAxisButtonsInCombos() {
    const auto game = BuildGameDefinition();

    trajectory::mapping::ActionMappingProfile profile;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.profile_name = "default";
    profile.axis_button_thresholds = trajectory::mapping::BuildDefaultAxisButtonThresholds();
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Combo({
                                              trajectory::mapping::ComboComponent::AxisButton("leftx", "positive"),
                                              trajectory::mapping::ComboComponent::Button("south"),
                                          })}});

    const auto validation = trajectory::mapping::ValidateProfile(game, profile);

    bool found_error = false;
    for (const auto& issue : validation.issues) {
        if (issue.severity == trajectory::mapping::ValidationSeverity::error) {
            found_error = true;
            break;
        }
    }
    Expect(!found_error, "directional axis button combos should be accepted");
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
    TestValidationAcceptsStickBindingForVector2Action();
    TestValidationRejectsAxisBindingForVector2Action();
    TestValidationRejectsStickBindingForAnalogAction();
    TestValidationFindsDuplicateStickBindingsAcrossActions();
    TestDescribeBindingFormatsStickBinding();
    TestValidationFindsDuplicateComboBindingsAcrossActions();
    TestValidationRejectsOversizedAndDuplicateComboMembers();
    TestValidationAcceptsDirectionalAxisButtonsInCombos();
    return 0;
}
