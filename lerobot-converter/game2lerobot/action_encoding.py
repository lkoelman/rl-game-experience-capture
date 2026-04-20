"""High-level action vector construction.

This module converts aligned raw gamepad state into the dense float action
vector consumed by the output LeRobot dataset. It owns the stable slot layout
and the v1 semantics for digital, analog, trigger, and vector2 actions.
"""

from __future__ import annotations

import numpy as np

from .constants import AXIS_NAME_TO_INDEX, BUTTON_NAME_TO_INDEX, STICK_NAME_TO_AXES
from .models import (
    ActionBinding,
    ActionDefinition,
    ActionLayoutEntry,
    BindingType,
    GamepadSnapshot,
)


def build_action_layout(actions: list[ActionDefinition]) -> list[ActionLayoutEntry]:
    """Build the deterministic flat vector layout for the ordered action list."""

    offset = 0
    layout: list[ActionLayoutEntry] = []
    for action in actions:
        size = 2 if action.kind == "vector2" else 1
        layout.append(
            ActionLayoutEntry(
                action_id=action.id, start=offset, size=size, kind=action.kind
            )
        )
        offset += size
    return layout


def encode_action_vector(
    layout: list[ActionLayoutEntry],
    bindings_by_action: dict[str, list[ActionBinding]],
    snapshot: GamepadSnapshot,
) -> np.ndarray:
    """Encode one aligned snapshot into the dense action vector written per frame."""

    encoded = np.zeros(sum(entry.size for entry in layout), dtype=np.float32)
    for entry in layout:
        bindings = bindings_by_action.get(entry.action_id, [])
        if not bindings:
            continue
        encoded[entry.start : entry.start + entry.size] = _evaluate_action(
            entry.kind, bindings, snapshot
        )
    return encoded


def collect_actions_by_class(
    game_classes: tuple, class_ids: tuple[str, ...]
) -> list[ActionDefinition]:
    """Flatten ordered class actions into the final output action sequence."""

    by_id = {game_class.id: game_class for game_class in game_classes}
    actions: list[ActionDefinition] = []
    for class_id in class_ids:
        actions.extend(by_id[class_id].actions)
    return actions


def _evaluate_action(
    kind: str, bindings: list[ActionBinding], snapshot: GamepadSnapshot
) -> np.ndarray:
    if kind == "vector2":
        return _evaluate_vector2(bindings, snapshot)
    if kind == "digital":
        return np.array(
            [max(_evaluate_digital(binding, snapshot) for binding in bindings)],
            dtype=np.float32,
        )
    if kind == "analog":
        return np.array(
            [
                _first_nonzero(
                    _evaluate_analog(binding, snapshot) for binding in bindings
                )
            ],
            dtype=np.float32,
        )
    if kind == "trigger":
        return np.array(
            [max(_evaluate_trigger(binding, snapshot) for binding in bindings)],
            dtype=np.float32,
        )
    raise ValueError(f"Unsupported action kind: {kind}")


def _evaluate_vector2(
    bindings: list[ActionBinding], snapshot: GamepadSnapshot
) -> np.ndarray:
    for binding in bindings:
        if binding.type == BindingType.STICK:
            x_axis, y_axis = STICK_NAME_TO_AXES[binding.control]
            return np.array(
                [_axis_value(snapshot, x_axis), _axis_value(snapshot, y_axis)],
                dtype=np.float32,
            )
    return np.zeros(2, dtype=np.float32)


def _evaluate_digital(binding: ActionBinding, snapshot: GamepadSnapshot) -> float:
    if binding.type == BindingType.BUTTON:
        return float(
            BUTTON_NAME_TO_INDEX.get(binding.control) in snapshot.pressed_buttons
        )
    if binding.type == BindingType.TRIGGER:
        return float(_axis_value(snapshot, binding.control) >= binding.threshold)
    if binding.type == BindingType.AXIS:
        return float(
            _axis_matches_direction(
                _axis_value(snapshot, binding.control), binding.direction
            )
        )
    if binding.type == BindingType.COMBO:
        return float(
            all(
                _combo_component_active(component, snapshot)
                for component in binding.controls
            )
        )
    return 0.0


def _evaluate_analog(binding: ActionBinding, snapshot: GamepadSnapshot) -> float:
    if binding.type != BindingType.AXIS:
        return 0.0
    value = _axis_value(snapshot, binding.control)
    if binding.direction == "positive":
        return max(0.0, value)
    if binding.direction == "negative":
        return abs(min(0.0, value))
    return value


def _evaluate_trigger(binding: ActionBinding, snapshot: GamepadSnapshot) -> float:
    if binding.type == BindingType.TRIGGER:
        value = _axis_value(snapshot, binding.control)
        return value if value >= binding.threshold else 0.0
    return 0.0


def _combo_component_active(
    component: dict[str, str], snapshot: GamepadSnapshot
) -> bool:
    component_type = component["type"]
    control = component["control"]
    if component_type == "button":
        return BUTTON_NAME_TO_INDEX.get(control) in snapshot.pressed_buttons
    if component_type == "axis_button":
        value = _axis_value(snapshot, control)
        direction = component.get("direction", "")
        if control.endswith("trigger"):
            return value >= 0.5
        if direction == "negative":
            return value <= -0.5
        return value >= 0.5
    return False


def _axis_matches_direction(value: float, direction: str) -> bool:
    if direction == "positive":
        return value > 0.0
    if direction == "negative":
        return value < 0.0
    return value != 0.0


def _axis_value(snapshot: GamepadSnapshot, axis_name: str) -> float:
    axis_index = AXIS_NAME_TO_INDEX[axis_name]
    if axis_index >= len(snapshot.axes):
        if axis_name.endswith("trigger") and snapshot.axes:
            return float(snapshot.axes[-1])
        return 0.0
    return float(snapshot.axes[axis_index])


def _first_nonzero(values) -> float:
    for value in values:
        if value != 0.0:
            return value
    return 0.0
