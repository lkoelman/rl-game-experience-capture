"""Core domain models for the converter.

These dataclasses are the handoff format between YAML parsing, session parsing,
alignment, encoding, and dataset export. They make the converter pipeline
explicit and keep the LeRobot writer isolated from raw input files.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum

from lerobot.datasets.lerobot_dataset import LeRobotDataset


class BindingType(StrEnum):
    """Stable binding kinds stored in action-mapping profiles."""

    BUTTON = "button"
    AXIS = "axis"
    STICK = "stick"
    TRIGGER = "trigger"
    COMBO = "combo"


@dataclass(frozen=True)
class ActionDefinition:
    """One high-level game action from the game definition.

    Fields:
    - `id`: Stable action identifier used in profiles and metadata.
    - `kind`: Encoding shape for this action in the dense action vector.
    """

    id: str
    kind: str


@dataclass(frozen=True)
class ActionBinding:
    """One low-level controller binding from the mapping profile.

    Fields:
    - `type`: Binding interpretation rule.
    - `control`: Stable control name from the recorder/mapping tool.
    - `direction`: Optional axis direction qualifier for analog bindings.
    - `threshold`: Trigger activation threshold when the binding is thresholded.
    - `controls`: Combo components for simultaneous-control bindings.
    """

    type: BindingType
    control: str
    direction: str = "any"
    threshold: float = 0.5
    controls: tuple[dict[str, str], ...] = ()


@dataclass(frozen=True)
class ActionLayoutEntry:
    """One segment in the emitted dense action vector.

    Fields:
    - `action_id`: Source action identifier for this vector slice.
    - `start`: Inclusive start offset in the flat vector.
    - `size`: Number of float slots owned by this action.
    - `kind`: Source action kind that determined the slot count.
    """

    action_id: str
    start: int
    size: int
    kind: str


@dataclass(frozen=True)
class GamepadSnapshot:
    """Raw controller snapshot aligned against video frames during conversion.

    Fields:
    - `monotonic_ns`: Recorder monotonic timestamp.
    - `axes`: Raw analog axis values from `actions.bin`.
    - `pressed_buttons`: Pressed SDL gamepad button ids.
    - `pressed_keys`: Pressed keyboard ids. Present in source data but ignored in v1 encoding.
    """

    monotonic_ns: int
    axes: tuple[float, ...]
    pressed_buttons: tuple[int, ...]
    pressed_keys: tuple[int, ...]


@dataclass(frozen=True)
class SessionValidationResult:
    """Validation result for a discovered session directory.

    Fields:
    - `ok`: Whether the session has the minimal artifact set required for conversion.
    - `missing_files`: Missing artifact names used for batch reporting and strict-mode failures.
    """

    ok: bool
    missing_files: tuple[str, ...]


@dataclass(frozen=True)
class GameClass:
    """A selectable class grouping ordered actions in the game definition.

    Fields:
    - `id`: Stable class identifier used by the mapping profile.
    - `actions`: Ordered actions contributed by this class to the output vector.
    """

    id: str
    actions: tuple[ActionDefinition, ...]


@dataclass(frozen=True)
class GameDefinition:
    """Parsed game action catalog used to build the output action space.

    Fields:
    - `game_id`: Stable game identifier shared with the mapping profile.
    - `display_name`: Human-readable game label preserved into dataset metadata.
    - `classes`: Ordered selectable classes and their actions.
    """

    game_id: str
    display_name: str
    classes: tuple[GameClass, ...]


@dataclass(frozen=True)
class ActionMappingProfile:
    """Parsed action-mapping profile for one operator/control setup.

    Fields:
    - `game_id`: Game identifier the profile was created for.
    - `class_ids`: Ordered class selection that defines the action layout.
    - `profile_name`: Human-readable profile label.
    - `complete`: Advisory completeness flag from the mapper UI.
    - `bindings_by_action`: Binding lists keyed by action id.
    """

    game_id: str
    class_ids: tuple[str, ...]
    profile_name: str
    complete: bool
    bindings_by_action: dict[str, list[ActionBinding]]


@dataclass(frozen=True)
class ConversionMetadata:
    """Metadata persisted into `meta/info.json` under the converter extension block.

    Fields:
    - `game_id`, `class_ids`, `profile_name`: Source identity for the action space.
    - `task`, `strict`, `max_pre_action_seconds`: Conversion settings that affect dataset semantics.
    - `action_layout`: Description of the emitted dense action vector.
    - `converted_sessions`, `skipped_sessions`: Batch outcomes for provenance and debugging.
    """

    game_id: str
    class_ids: tuple[str, ...]
    profile_name: str
    task: str
    strict: bool
    max_pre_action_seconds: float
    action_layout: tuple[ActionLayoutEntry, ...]
    converted_sessions: tuple[str, ...]
    skipped_sessions: dict[str, str]


@dataclass(frozen=True)
class ConversionResult:
    """Result of one batch conversion run.

    Fields:
    - `converted_sessions`: Session names retained as dataset episodes.
    - `skipped_sessions`: Session names skipped during best-effort conversion and the reason.
    - `dataset`: Materialized LeRobot dataset handle for callers that need post-write inspection.
    """

    converted_sessions: tuple[str, ...]
    skipped_sessions: dict[str, str]
    dataset: LeRobotDataset | None = field(default=None, compare=False)
