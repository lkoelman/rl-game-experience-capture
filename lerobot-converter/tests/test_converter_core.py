from pathlib import Path

import av
import numpy as np
import pytest
import yaml
from google.protobuf import descriptor_pb2, descriptor_pool, message_factory

from game2lerobot import (
    ActionBinding,
    ActionDefinition,
    ActionLayoutEntry,
    BindingType,
    ConversionMetadata,
    ConversionResult,
    GamepadSnapshot,
    apply_converter_metadata,
    build_action_layout,
    collect_session_dirs,
    trim_idle_frame_indices,
    convert_sessions,
    encode_action_vector,
    load_action_mapping_profile,
    load_game_definition,
    read_actions_bin,
    read_sync_csv,
    validate_session_dir,
)


def test_collect_session_dirs_and_validate_required_files(tmp_path: Path):
    valid = tmp_path / "valid_session"
    valid.mkdir()
    for name in ("capture.mp4", "sync.csv", "actions.bin"):
        (valid / name).write_bytes(b"data")

    invalid = tmp_path / "invalid_session"
    invalid.mkdir()
    (invalid / "capture.mp4").write_bytes(b"data")

    sessions = collect_session_dirs(tmp_path)

    assert [session.name for session in sessions] == [
        "invalid_session",
        "valid_session",
    ]
    assert validate_session_dir(valid).ok is True
    assert validate_session_dir(invalid).ok is False
    assert validate_session_dir(invalid).missing_files == ("actions.bin", "sync.csv")


def test_trim_idle_frame_indices_uses_pre_action_limit():
    frame_timestamps = [1_000_000_000, 1_500_000_000, 1_800_000_000, 2_000_000_000]

    retained = trim_idle_frame_indices(
        frame_timestamps_ns=frame_timestamps,
        first_action_timestamp_ns=2_000_000_000,
        max_pre_action_seconds=0.6,
    )

    assert retained == [1, 2, 3]


def test_build_action_layout_and_encode_vector():
    actions = [
        ActionDefinition(id="move", kind="vector2"),
        ActionDefinition(id="strike", kind="digital"),
        ActionDefinition(id="aim", kind="analog"),
        ActionDefinition(id="heavy", kind="trigger"),
        ActionDefinition(id="unmapped", kind="digital"),
    ]
    layout = build_action_layout(actions)

    assert layout == [
        ActionLayoutEntry(action_id="move", start=0, size=2, kind="vector2"),
        ActionLayoutEntry(action_id="strike", start=2, size=1, kind="digital"),
        ActionLayoutEntry(action_id="aim", start=3, size=1, kind="analog"),
        ActionLayoutEntry(action_id="heavy", start=4, size=1, kind="trigger"),
        ActionLayoutEntry(action_id="unmapped", start=5, size=1, kind="digital"),
    ]

    snapshot = GamepadSnapshot(
        monotonic_ns=123,
        axes=(0.25, -0.75, 0.0, 0.0, 0.8),
        pressed_buttons=(1,),
        pressed_keys=(99,),
    )
    bindings = {
        "move": [ActionBinding(type=BindingType.STICK, control="left_stick")],
        "strike": [ActionBinding(type=BindingType.BUTTON, control="south")],
        "aim": [
            ActionBinding(type=BindingType.AXIS, control="leftx", direction="positive")
        ],
        "heavy": [
            ActionBinding(
                type=BindingType.TRIGGER, control="right_trigger", threshold=0.5
            )
        ],
        "unmapped": [],
    }

    encoded = encode_action_vector(
        layout=layout, bindings_by_action=bindings, snapshot=snapshot
    )

    assert np.allclose(
        encoded, np.array([0.25, -0.75, 1.0, 0.25, 0.8, 0.0], dtype=np.float32)
    )


def test_apply_converter_metadata_uses_namespaced_extension():
    info = {
        "fps": 30,
        "features": {"action": {"dtype": "float32", "shape": [3], "names": None}},
    }
    metadata = ConversionMetadata(
        game_id="path_of_exile_2",
        class_ids=("default", "warrior"),
        profile_name="default",
        task="Clear the zone",
        strict=False,
        max_pre_action_seconds=1.5,
        action_layout=(
            ActionLayoutEntry(action_id="move", start=0, size=2, kind="vector2"),
            ActionLayoutEntry(action_id="strike", start=2, size=1, kind="digital"),
        ),
        converted_sessions=("session_a",),
        skipped_sessions={"session_b": "missing actions.bin"},
    )

    apply_converter_metadata(info, metadata)

    assert "extensions" in info
    assert "game_converter" in info["extensions"]
    assert info["extensions"]["game_converter"]["game_id"] == "path_of_exile_2"
    assert info["extensions"]["game_converter"]["settings"]["task"] == "Clear the zone"
    assert (
        info["extensions"]["game_converter"]["action_layout"][0]["action_id"] == "move"
    )


def test_read_sync_csv_and_actions_bin_round_trip(tmp_path: Path):
    sync_path = tmp_path / "sync.csv"
    sync_path.write_text("frame_index,monotonic_ns,pts\n0,1000,0\n1,2000,33333333\n")

    actions_path = tmp_path / "actions.bin"
    _write_actions_bin(
        actions_path,
        [
            GamepadSnapshot(
                monotonic_ns=900,
                axes=(0.1, -0.2),
                pressed_buttons=(1,),
                pressed_keys=(8,),
            ),
            GamepadSnapshot(
                monotonic_ns=1900,
                axes=(0.3, 0.4),
                pressed_buttons=(2,),
                pressed_keys=(),
            ),
        ],
    )

    assert read_sync_csv(sync_path) == [1000, 2000]

    actions = read_actions_bin(actions_path)

    assert [action.monotonic_ns for action in actions] == [900, 1900]
    assert np.allclose(actions[0].axes, np.array([0.1, -0.2], dtype=np.float32))
    assert np.allclose(actions[1].axes, np.array([0.3, 0.4], dtype=np.float32))
    assert actions[0].pressed_buttons == (1,)
    assert actions[0].pressed_keys == (8,)
    assert actions[1].pressed_buttons == (2,)
    assert actions[1].pressed_keys == ()


def test_convert_sessions_writes_dataset_and_metadata(tmp_path: Path):
    batch_root = tmp_path / "sessions"
    batch_root.mkdir()
    valid = batch_root / "session_valid"
    valid.mkdir()
    invalid = batch_root / "session_invalid"
    invalid.mkdir()
    (invalid / "capture.mp4").write_bytes(b"broken")

    _write_video(
        valid / "capture.mp4",
        [
            np.full((8, 8, 3), 10, dtype=np.uint8),
            np.full((8, 8, 3), 20, dtype=np.uint8),
        ],
        fps=30,
    )
    (valid / "sync.csv").write_text(
        "frame_index,monotonic_ns,pts\n0,1000000000,0\n1,1033333333,33333333\n"
    )
    _write_actions_bin(
        valid / "actions.bin",
        [
            GamepadSnapshot(
                monotonic_ns=1010000000,
                axes=(0.5, -0.5, 0.0, 0.0, 0.9),
                pressed_buttons=(1,),
                pressed_keys=(42,),
            ),
        ],
    )

    game_definition_path = tmp_path / "game-definition.yaml"
    game_definition_path.write_text(
        yaml.safe_dump(
            {
                "game_id": "test_game",
                "display_name": "Test Game",
                "classes": [
                    {
                        "id": "default",
                        "label": "Default",
                        "actions": [
                            {"id": "move", "label": "Move", "kind": "vector2"},
                            {"id": "attack", "label": "Attack", "kind": "digital"},
                            {"id": "heavy", "label": "Heavy", "kind": "trigger"},
                        ],
                    }
                ],
            }
        )
    )
    action_mapping_path = tmp_path / "action-mapping.yaml"
    action_mapping_path.write_text(
        yaml.safe_dump(
            {
                "game_id": "test_game",
                "class_ids": ["default"],
                "profile_name": "test-profile",
                "complete": False,
                "actions": {
                    "move": {
                        "skipped": False,
                        "bindings": [{"type": "stick", "control": "left_stick"}],
                    },
                    "attack": {
                        "skipped": False,
                        "bindings": [{"type": "button", "control": "south"}],
                    },
                    "heavy": {
                        "skipped": False,
                        "bindings": [
                            {
                                "type": "trigger",
                                "control": "right_trigger",
                                "threshold": 0.5,
                            }
                        ],
                    },
                },
            }
        )
    )

    result = convert_sessions(
        session_root=batch_root,
        game_definition=load_game_definition(game_definition_path),
        action_mapping=load_action_mapping_profile(action_mapping_path),
        output_root=tmp_path / "out",
        repo_id="local/test_dataset",
        task="Defeat enemies",
        max_pre_action_seconds=0.1,
        strict=False,
    )

    assert result == ConversionResult(
        converted_sessions=("session_valid",),
        skipped_sessions={
            "session_invalid": "missing required files: actions.bin, sync.csv"
        },
    )
    info = result.dataset.meta.info
    assert info["fps"] == 30
    assert info["total_episodes"] == 1
    assert info["features"]["observation.images.main"]["dtype"] == "video"
    assert info["features"]["action"]["shape"] == (4,)
    assert info["extensions"]["game_converter"]["converted_sessions"] == [
        "session_valid"
    ]
    assert (
        info["extensions"]["game_converter"]["skipped_sessions"]["session_invalid"]
        == "missing required files: actions.bin, sync.csv"
    )
    assert info["extensions"]["game_converter"]["settings"]["task"] == "Defeat enemies"
    assert (
        info["extensions"]["game_converter"]["action_layout"][2]["action_id"] == "heavy"
    )


def test_convert_sessions_strict_mode_fails_on_invalid_session(tmp_path: Path):
    batch_root = tmp_path / "sessions"
    batch_root.mkdir()
    invalid = batch_root / "session_invalid"
    invalid.mkdir()
    (invalid / "capture.mp4").write_bytes(b"broken")

    game_definition_path = tmp_path / "game-definition.yaml"
    game_definition_path.write_text(
        yaml.safe_dump(
            {
                "game_id": "test_game",
                "display_name": "Test Game",
                "classes": [{"id": "default", "label": "Default", "actions": []}],
            }
        )
    )
    action_mapping_path = tmp_path / "action-mapping.yaml"
    action_mapping_path.write_text(
        yaml.safe_dump(
            {
                "game_id": "test_game",
                "class_ids": ["default"],
                "profile_name": "test-profile",
                "actions": {},
            }
        )
    )

    with pytest.raises(
        ValueError,
        match="session_invalid: missing required files: actions.bin, sync.csv",
    ):
        convert_sessions(
            session_root=batch_root,
            game_definition=load_game_definition(game_definition_path),
            action_mapping=load_action_mapping_profile(action_mapping_path),
            output_root=tmp_path / "out",
            repo_id="local/test_dataset",
            task="Defeat enemies",
            max_pre_action_seconds=0.1,
            strict=True,
        )


def _write_video(path: Path, frames: list[np.ndarray], fps: int) -> None:
    with av.open(str(path), "w") as container:
        stream = container.add_stream("mpeg4", rate=fps)
        stream.width = frames[0].shape[1]
        stream.height = frames[0].shape[0]
        stream.pix_fmt = "yuv420p"
        for frame_array in frames:
            frame = av.VideoFrame.from_ndarray(frame_array, format="rgb24")
            for packet in stream.encode(frame):
                container.mux(packet)
        for packet in stream.encode():
            container.mux(packet)


def _write_actions_bin(path: Path, snapshots: list[GamepadSnapshot]) -> None:
    message_cls = _build_gamepad_state_message()
    with path.open("wb") as handle:
        for snapshot in snapshots:
            message = message_cls()
            message.monotonic_ns = snapshot.monotonic_ns
            message.axes.extend(snapshot.axes)
            message.pressed_buttons.extend(snapshot.pressed_buttons)
            message.pressed_keys.extend(snapshot.pressed_keys)
            payload = message.SerializeToString()
            handle.write(len(payload).to_bytes(4, "little"))
            handle.write(payload)


def _build_gamepad_state_message():
    file_descriptor = descriptor_pb2.FileDescriptorProto()
    file_descriptor.name = "gamepad.proto"
    file_descriptor.package = "trajectory"
    message = file_descriptor.message_type.add()
    message.name = "GamepadState"

    field = message.field.add()
    field.name = "monotonic_ns"
    field.number = 1
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_UINT64

    field = message.field.add()
    field.name = "axes"
    field.number = 2
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT

    field = message.field.add()
    field.name = "pressed_buttons"
    field.number = 3
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_UINT32

    field = message.field.add()
    field.name = "pressed_keys"
    field.number = 4
    field.label = descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
    field.type = descriptor_pb2.FieldDescriptorProto.TYPE_UINT32

    pool = descriptor_pool.DescriptorPool()
    pool.Add(file_descriptor)
    descriptor = pool.FindMessageTypeByName("trajectory.GamepadState")
    return message_factory.GetMessageClass(descriptor)
