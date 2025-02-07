"""
Configuration of a PoE2 experience (recording).
"""

from dataclasses import dataclass
from typing import Optional
from enum import StrEnum
from pathlib import Path

import hydra
from hydra.core.config_store import ConfigStore

from .game_state import CharacterInfo, WorldLocation

# - [ ] implement game metadata file
#     - gamepad -> action mapping
#     - level info
#     - graphics info
#     - game version info
#     - class/character info
#     - weapon/loadout info


# See https://hydra.cc/docs/tutorials/structured_config/hierarchical_static_config/

@dataclass
class GraphicsSettings:
    pass # TODO

class GameDifficulty(StrEnum):
    pass # TODO

@dataclass
class ExperienceCapture:
    author: Optional[str] = None
    game_version: str
    character: CharacterInfo
    graphics: GraphicsSettings
    difficulty: GameDifficulty
    starting_location: WorldLocation
    videos: list[Path]
    frame_timestamps: list[Path]  # one frame timestamp file per video file
    gamepad_state: Path