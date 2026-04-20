#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "ActionMapping.hpp"
#include "ActionMappingYaml.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path WriteTempFile(const std::string& name, const std::string& contents) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
    return path;
}

void TestGameDefinitionParsesFromYaml() {
    const auto path = WriteTempFile(
        "game-actions-test.yaml",
        "game_id: demo\n"
        "display_name: Demo Game\n"
        "classes:\n"
        "  - id: mage\n"
        "    label: Mage\n"
        "    actions:\n"
        "      - id: jump\n"
        "        label: Jump\n"
        "        kind: digital\n"
        "        required: true\n"
        "      - id: move_character\n"
        "        label: Move Character\n"
        "        kind: vector2\n"
        "      - id: cast_fireball\n"
        "        label: Cast Fireball\n"
        "        kind: digital\n");

    const auto game = trajectory::mapping::LoadGameDefinition(path.string());

    Expect(game.game_id == "demo", "game id should parse");
    Expect(game.classes.size() == 1, "class list should parse");
    Expect(game.classes[0].actions.size() == 3, "class actions should parse");
    Expect(game.classes[0].actions[1].kind == trajectory::mapping::ActionInputKind::vector2, "vector2 actions should parse");
}

void TestProfileRoundTripsToYaml() {
    trajectory::mapping::ActionMappingProfile profile;
    profile.schema_version = 1;
    profile.game_id = "demo";
    profile.class_ids = {"mage", "archer"};
    profile.profile_name = "steam-deck";
    profile.complete = true;
    profile.axis_button_thresholds = trajectory::mapping::BuildDefaultAxisButtonThresholds();
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});
    profile.actions.push_back({"move_x", false, {trajectory::mapping::ActionBinding::Axis("leftx", "any")}});
    profile.actions.push_back({"move_character", false, {trajectory::mapping::ActionBinding::Stick("left_stick")}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Trigger("right_trigger", 0.65f)}});

    const auto path = WriteTempFile("action-mapping-test.yaml", "");
    trajectory::mapping::SaveActionMappingProfile(profile, path.string());
    const auto loaded = trajectory::mapping::LoadActionMappingProfile(path.string());

    Expect(loaded.profile_name == "steam-deck", "profile name should round-trip");
    Expect(loaded.actions.size() == 4, "all actions should round-trip");
    Expect(loaded.class_ids.size() == 2, "multiple class ids should round-trip");
    Expect(loaded.class_ids[1] == "archer", "class id order should round-trip");
    Expect(loaded.actions[2].bindings[0].type == trajectory::mapping::BindingType::stick, "stick bindings should round-trip");
    Expect(loaded.actions[3].bindings[0].threshold == 0.65f, "trigger thresholds should round-trip");
}

void TestComboProfileRoundTripsWithAxisButtonThresholds() {
    trajectory::mapping::ActionMappingProfile profile;
    profile.schema_version = 1;
    profile.game_id = "demo";
    profile.class_ids = {"mage"};
    profile.profile_name = "steam-deck";
    profile.complete = true;
    profile.axis_button_thresholds = trajectory::mapping::BuildDefaultAxisButtonThresholds();
    profile.axis_button_thresholds[0].threshold = 0.65f;
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Combo({
                                              trajectory::mapping::ComboComponent::Button("south"),
                                              trajectory::mapping::ComboComponent::AxisButton("left_trigger", ""),
                                          })}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Combo({
                                                        trajectory::mapping::ComboComponent::AxisButton("leftx", "positive"),
                                                        trajectory::mapping::ComboComponent::Button("east"),
                                                    })}});

    const auto path = WriteTempFile("action-mapping-combo-test.yaml", "");
    trajectory::mapping::SaveActionMappingProfile(profile, path.string());
    const auto loaded = trajectory::mapping::LoadActionMappingProfile(path.string());

    Expect(loaded.axis_button_thresholds.size() == profile.axis_button_thresholds.size(), "axis button thresholds should round-trip");
    Expect(trajectory::mapping::ResolveAxisButtonThreshold(loaded, "left_trigger") == 0.65f, "configured axis threshold should round-trip");
    Expect(loaded.actions[0].bindings[0].type == trajectory::mapping::BindingType::combo, "combo binding should round-trip");
    Expect(loaded.actions[0].bindings[0].combo_components.size() == 2, "combo members should round-trip");
    Expect(loaded.actions[1].bindings[0].combo_components[0].direction == "positive", "directional axis combo member should round-trip");
}

void TestMissingAxisButtonThresholdsDefaultOnLoad() {
    const auto path = WriteTempFile(
        "action-mapping-combo-default-thresholds.yaml",
        "schema_version: 1\n"
        "game_id: demo\n"
        "class_ids:\n"
        "  - mage\n"
        "profile_name: default\n"
        "complete: true\n"
        "actions:\n"
        "  jump:\n"
        "    bindings:\n"
        "      - type: combo\n"
        "        controls:\n"
        "          - type: button\n"
        "            control: south\n"
        "          - type: axis_button\n"
        "            control: left_trigger\n");

    const auto loaded = trajectory::mapping::LoadActionMappingProfile(path.string());

    Expect(trajectory::mapping::ResolveAxisButtonThreshold(loaded, "left_trigger") == trajectory::mapping::kDefaultAxisButtonThreshold,
           "missing axis button thresholds should default on load");
}

void TestMissingClassIdsFailsClearly() {
    const auto path = WriteTempFile(
        "action-mapping-missing-class-ids.yaml",
        "schema_version: 1\n"
        "game_id: demo\n"
        "class_id: mage\n"
        "profile_name: default\n"
        "complete: true\n");

    bool threw = false;
    try {
        static_cast<void>(trajectory::mapping::LoadActionMappingProfile(path.string()));
    } catch (const std::exception& error) {
        threw = std::string(error.what()).find("class_ids") != std::string::npos;
    }
    Expect(threw, "profiles without class_ids should fail clearly");
}

void TestInvalidThresholdFailsClearly() {
    const auto path = WriteTempFile(
        "action-mapping-invalid.yaml",
        "schema_version: 1\n"
        "game_id: demo\n"
        "class_ids:\n"
        "  - mage\n"
        "profile_name: default\n"
        "complete: true\n"
        "actions:\n"
        "  jump:\n"
        "    bindings:\n"
        "      - type: trigger\n"
        "        control: left_trigger\n"
        "        threshold: 1.5\n");

    bool threw = false;
    try {
        static_cast<void>(trajectory::mapping::LoadActionMappingProfile(path.string()));
    } catch (const std::exception& error) {
        threw = std::string(error.what()).find("threshold") != std::string::npos;
    }

    Expect(threw, "invalid trigger threshold should fail with a clear error");
}

void TestInvalidStickControlFailsClearly() {
    const auto path = WriteTempFile(
        "action-mapping-invalid-stick.yaml",
        "schema_version: 1\n"
        "game_id: demo\n"
        "class_ids:\n"
        "  - mage\n"
        "profile_name: default\n"
        "complete: true\n"
        "actions:\n"
        "  move_character:\n"
        "    bindings:\n"
        "      - type: stick\n"
        "        control: leftx\n");

    const auto loaded = trajectory::mapping::LoadActionMappingProfile(path.string());
    Expect(loaded.actions[0].bindings[0].type == trajectory::mapping::BindingType::stick, "stick bindings should parse before validation");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestGameDefinitionParsesFromYaml();
    TestProfileRoundTripsToYaml();
    TestComboProfileRoundTripsWithAxisButtonThresholds();
    TestMissingAxisButtonThresholdsDefaultOnLoad();
    TestMissingClassIdsFailsClearly();
    TestInvalidThresholdFailsClearly();
    TestInvalidStickControlFailsClearly();
    return 0;
}
