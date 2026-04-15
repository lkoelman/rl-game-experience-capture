#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "ActionMapping.hpp"
#include "ActionMappingYaml.hpp"

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
        "    specializations:\n"
        "      - id: fire\n"
        "        label: Fire\n"
        "        actions:\n"
        "          - id: cast_fireball\n"
        "            label: Cast Fireball\n"
        "            kind: digital\n");

    const auto game = trajectory::mapping::LoadGameDefinition(path.string());

    Expect(game.game_id == "demo", "game id should parse");
    Expect(game.classes.size() == 1, "class list should parse");
    Expect(game.classes[0].specializations.size() == 1, "specialization list should parse");
}

void TestProfileRoundTripsToYaml() {
    trajectory::mapping::ActionMappingProfile profile;
    profile.schema_version = 1;
    profile.game_id = "demo";
    profile.class_id = "mage";
    profile.spec_id = "fire";
    profile.profile_name = "steam-deck";
    profile.complete = true;
    profile.actions.push_back({"jump", false, {trajectory::mapping::ActionBinding::Button("south")}});
    profile.actions.push_back({"move_x", false, {trajectory::mapping::ActionBinding::Axis("leftx", "any")}});
    profile.actions.push_back({"cast_fireball", false, {trajectory::mapping::ActionBinding::Trigger("right_trigger", 0.65f)}});

    const auto path = WriteTempFile("action-mapping-test.yaml", "");
    trajectory::mapping::SaveActionMappingProfile(profile, path.string());
    const auto loaded = trajectory::mapping::LoadActionMappingProfile(path.string());

    Expect(loaded.profile_name == "steam-deck", "profile name should round-trip");
    Expect(loaded.actions.size() == 3, "all actions should round-trip");
    Expect(loaded.actions[2].bindings[0].threshold == 0.65f, "trigger thresholds should round-trip");
}

void TestInvalidThresholdFailsClearly() {
    const auto path = WriteTempFile(
        "action-mapping-invalid.yaml",
        "schema_version: 1\n"
        "game_id: demo\n"
        "class_id: mage\n"
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

}  // namespace

int main() {
    TestGameDefinitionParsesFromYaml();
    TestProfileRoundTripsToYaml();
    TestInvalidThresholdFailsClearly();
    return 0;
}
