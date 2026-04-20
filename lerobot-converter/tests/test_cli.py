from pathlib import Path

from main import main


def test_main_parses_batch_cli_arguments(monkeypatch, tmp_path: Path):
    captured = {}

    def fake_load_game_definition(path):
        captured["game_definition_path"] = path
        return "game-definition"

    def fake_load_action_mapping_profile(path):
        captured["action_mapping_path"] = path
        return "action-mapping"

    def fake_convert_sessions(**kwargs):
        captured["convert_kwargs"] = kwargs

    monkeypatch.setattr("main.load_game_definition", fake_load_game_definition)
    monkeypatch.setattr("main.load_action_mapping_profile", fake_load_action_mapping_profile)
    monkeypatch.setattr("main.convert_sessions", fake_convert_sessions)

    main(
        [
            "--session-root",
            str(tmp_path / "sessions"),
            "--game-definition",
            str(tmp_path / "game.yaml"),
            "--action-mapping",
            str(tmp_path / "mapping.yaml"),
            "--output-root",
            str(tmp_path / "out"),
            "--repo-id",
            "local/test_dataset",
            "--task",
            "Defeat enemies",
            "--max-pre-action-seconds",
            "0.25",
            "--strict",
        ]
    )

    assert captured["game_definition_path"] == tmp_path / "game.yaml"
    assert captured["action_mapping_path"] == tmp_path / "mapping.yaml"
    assert captured["convert_kwargs"]["session_root"] == tmp_path / "sessions"
    assert captured["convert_kwargs"]["output_root"] == tmp_path / "out"
    assert captured["convert_kwargs"]["repo_id"] == "local/test_dataset"
    assert captured["convert_kwargs"]["task"] == "Defeat enemies"
    assert captured["convert_kwargs"]["max_pre_action_seconds"] == 0.25
    assert captured["convert_kwargs"]["strict"] is True
