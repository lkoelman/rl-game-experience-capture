from pathlib import Path

from game2lerobot.cli import main


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

    monkeypatch.setattr(
        "game2lerobot.cli.load_game_definition", fake_load_game_definition
    )
    monkeypatch.setattr(
        "game2lerobot.cli.load_action_mapping_profile", fake_load_action_mapping_profile
    )
    monkeypatch.setattr("game2lerobot.cli.convert_sessions", fake_convert_sessions)

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
            "--no-reencode",
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
    assert captured["convert_kwargs"]["no_reencode"] is True
    assert captured["convert_kwargs"]["strict"] is True


def test_main_makes_pre_action_trim_optional(monkeypatch, tmp_path: Path):
    captured = {}

    monkeypatch.setattr("game2lerobot.cli.load_game_definition", lambda _path: "game")
    monkeypatch.setattr(
        "game2lerobot.cli.load_action_mapping_profile", lambda _path: "mapping"
    )
    monkeypatch.setattr(
        "game2lerobot.cli.convert_sessions",
        lambda **kwargs: captured.setdefault("convert_kwargs", kwargs),
    )

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
        ]
    )

    assert captured["convert_kwargs"]["max_pre_action_seconds"] is None
    assert captured["convert_kwargs"]["no_reencode"] is False
