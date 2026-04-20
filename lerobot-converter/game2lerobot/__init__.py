"""Public package surface for the gameplay-to-LeRobot converter."""

from .action_encoding import (
    build_action_layout,
    collect_actions_by_class,
    encode_action_vector,
)
from .alignment import (
    collect_session_dirs,
    trim_idle_frame_indices,
    validate_session_dir,
)
from .metadata import apply_converter_metadata, build_features
from .models import (
    ActionBinding,
    ActionDefinition,
    ActionLayoutEntry,
    ActionMappingProfile,
    BindingType,
    ConversionMetadata,
    ConversionResult,
    GameClass,
    GameDefinition,
    GamepadSnapshot,
    SessionValidationResult,
)
from .parsing import (
    load_action_mapping_profile,
    load_game_definition,
    open_video_reader,
    read_actions_bin,
    read_sync_csv,
)
from .pipeline import convert_sessions

__all__ = [
    "ActionBinding",
    "ActionDefinition",
    "ActionLayoutEntry",
    "ActionMappingProfile",
    "BindingType",
    "ConversionMetadata",
    "ConversionResult",
    "GameClass",
    "GameDefinition",
    "GamepadSnapshot",
    "SessionValidationResult",
    "apply_converter_metadata",
    "build_action_layout",
    "build_features",
    "collect_actions_by_class",
    "collect_session_dirs",
    "trim_idle_frame_indices",
    "convert_sessions",
    "encode_action_vector",
    "load_action_mapping_profile",
    "load_game_definition",
    "open_video_reader",
    "read_actions_bin",
    "read_sync_csv",
    "validate_session_dir",
]
