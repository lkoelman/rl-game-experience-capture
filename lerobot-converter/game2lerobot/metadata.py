"""Dataset metadata helpers.

These functions define the converter-owned extension block in LeRobot
`meta/info.json` and the output feature schema shared by dataset creation and
tests.
"""

from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .models import ActionLayoutEntry, ConversionMetadata


def apply_converter_metadata(info: dict, metadata: ConversionMetadata) -> None:
    """Inject the converter extension block into LeRobot `info.json` content."""

    info.setdefault("extensions", {})
    info["extensions"]["game_converter"] = {
        "game_id": metadata.game_id,
        "class_ids": list(metadata.class_ids),
        "profile_name": metadata.profile_name,
        "settings": {
            "task": metadata.task,
            "strict": metadata.strict,
            "max_pre_action_seconds": metadata.max_pre_action_seconds,
        },
        "action_layout": [asdict(entry) for entry in metadata.action_layout],
        "converted_sessions": list(metadata.converted_sessions),
        "skipped_sessions": metadata.skipped_sessions,
    }


def build_features(
    layout: list[ActionLayoutEntry], frame_shape: tuple[int, int, int]
) -> dict[str, dict[str, Any]]:
    """Create the v1 LeRobot feature schema for video-only observations."""

    return {
        "observation.images.main": {
            "dtype": "video",
            "shape": frame_shape,
            "names": ["height", "width", "channels"],
        },
        "action": {
            "dtype": "float32",
            "shape": (sum(entry.size for entry in layout),),
            "names": {
                "axes": [entry.action_id for entry in layout for _ in range(entry.size)]
            },
        },
    }
