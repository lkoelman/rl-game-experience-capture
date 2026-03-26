"""
Extract Path of Exile 2 game state from video recording.
"""

from .game_state import PlayerStats, UiStatus


# Steps:
# - load config describing metadata such as start time, FPS, resolution, graphics settings, gamepad/keyboard mode, etc.
# - use decord to scroll through video and extract the state, frame-by-frame
#   - see game state extraction pipeline: convert C++ approach to Python
#       - can use PIL instead of more heavy OpenCV
# - save to csv/pth/parquet/...