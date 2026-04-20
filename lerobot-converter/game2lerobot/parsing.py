"""Parsers for converter inputs.

This module is the boundary between on-disk artifacts and the in-memory domain
model used by the pipeline. It keeps YAML, CSV, binary protobuf, and video I/O
out of the orchestration layer.
"""

from __future__ import annotations

import csv
from pathlib import Path

import av
import numpy as np
import yaml
from google.protobuf import descriptor_pb2, descriptor_pool, message_factory

from .models import (
    ActionBinding,
    ActionDefinition,
    ActionMappingProfile,
    BindingType,
    GameClass,
    GameDefinition,
    GamepadSnapshot,
)

_GAMEPAD_STATE_MESSAGE = None


def read_sync_csv(path: Path) -> list[int]:
    """Read recorder frame timestamps from `sync.csv` in capture order."""

    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        return [int(row["monotonic_ns"]) for row in reader]


def read_actions_bin(path: Path) -> list[GamepadSnapshot]:
    """Read length-prefixed protobuf snapshots from `actions.bin`."""

    message_cls = _get_gamepad_state_message()
    snapshots: list[GamepadSnapshot] = []
    with path.open("rb") as handle:
        while True:
            length_bytes = handle.read(4)
            if not length_bytes:
                break
            payload_length = int.from_bytes(length_bytes, "little")
            payload = handle.read(payload_length)
            message = message_cls()
            message.ParseFromString(payload)
            snapshots.append(
                GamepadSnapshot(
                    monotonic_ns=int(message.monotonic_ns),
                    axes=tuple(float(value) for value in message.axes),
                    pressed_buttons=tuple(
                        int(value) for value in message.pressed_buttons
                    ),
                    pressed_keys=tuple(int(value) for value in message.pressed_keys),
                )
            )
    return snapshots


def load_game_definition(path: Path) -> GameDefinition:
    """Parse the game action catalog used to define the output action space."""

    raw = yaml.safe_load(path.read_text())
    classes = []
    for class_entry in raw["classes"]:
        actions = tuple(
            ActionDefinition(id=action["id"], kind=action["kind"])
            for action in class_entry.get("actions", [])
        )
        classes.append(GameClass(id=class_entry["id"], actions=actions))
    return GameDefinition(
        game_id=raw["game_id"],
        display_name=raw.get("display_name", ""),
        classes=tuple(classes),
    )


def load_action_mapping_profile(path: Path) -> ActionMappingProfile:
    """Parse the action mapping profile emitted by the mapper UI."""

    raw = yaml.safe_load(path.read_text())
    bindings_by_action: dict[str, list[ActionBinding]] = {}
    for action_id, entry in raw.get("actions", {}).items():
        bindings = []
        for binding in entry.get("bindings", []) or []:
            bindings.append(
                ActionBinding(
                    type=BindingType(binding["type"]),
                    control=binding.get("control", ""),
                    direction=binding.get("direction", "any"),
                    threshold=float(binding.get("threshold", 0.5)),
                    controls=tuple(binding.get("controls", ())),
                )
            )
        bindings_by_action[action_id] = bindings
    return ActionMappingProfile(
        game_id=raw["game_id"],
        class_ids=tuple(raw.get("class_ids", ())),
        profile_name=raw.get("profile_name", ""),
        complete=bool(raw.get("complete", False)),
        bindings_by_action=bindings_by_action,
    )


def read_video_frames(path: Path) -> tuple[list[np.ndarray], int]:
    """Decode all video frames and the capture FPS from the recorded session video."""

    with av.open(str(path)) as container:
        stream = container.streams.video[0]
        fps = int(round(float(stream.average_rate or stream.base_rate)))
        frames = [
            frame.to_ndarray(format="rgb24") for frame in container.decode(stream)
        ]
    return frames, fps


def _get_gamepad_state_message():
    global _GAMEPAD_STATE_MESSAGE
    if _GAMEPAD_STATE_MESSAGE is None:
        file_descriptor = descriptor_pb2.FileDescriptorProto()
        file_descriptor.name = "gamepad.proto"
        file_descriptor.package = "trajectory"
        message = file_descriptor.message_type.add()
        message.name = "GamepadState"
        for name, number, field_type, label in (
            (
                "monotonic_ns",
                1,
                descriptor_pb2.FieldDescriptorProto.TYPE_UINT64,
                descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL,
            ),
            (
                "axes",
                2,
                descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT,
                descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED,
            ),
            (
                "pressed_buttons",
                3,
                descriptor_pb2.FieldDescriptorProto.TYPE_UINT32,
                descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED,
            ),
            (
                "pressed_keys",
                4,
                descriptor_pb2.FieldDescriptorProto.TYPE_UINT32,
                descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED,
            ),
        ):
            field = message.field.add()
            field.name = name
            field.number = number
            field.type = field_type
            field.label = label
        pool = descriptor_pool.DescriptorPool()
        pool.Add(file_descriptor)
        descriptor = pool.FindMessageTypeByName("trajectory.GamepadState")
        _GAMEPAD_STATE_MESSAGE = message_factory.GetMessageClass(descriptor)
    return _GAMEPAD_STATE_MESSAGE
