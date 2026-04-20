"""Compatibility facade for the converter package.

Historically the package exposed its public API from a single module. The
implementation now lives in smaller modules grouped by concern, while this file
preserves the old import surface for tests and callers.
"""

from .alignment import collect_session_dirs, compute_retained_frame_indices, validate_session_dir
from .action_encoding import build_action_layout, collect_actions_by_class, encode_action_vector
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
    read_actions_bin,
    read_sync_csv,
    read_video_frames,
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
    "compute_retained_frame_indices",
    "convert_sessions",
    "encode_action_vector",
    "load_action_mapping_profile",
    "load_game_definition",
    "read_actions_bin",
    "read_sync_csv",
    "read_video_frames",
    "validate_session_dir",
]
