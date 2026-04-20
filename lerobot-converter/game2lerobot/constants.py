"""Shared protocol constants for recorded gamepad sessions.

This module centralizes the stable file names and SDL-derived control mappings
used across parsing and encoding so the converter keeps one interpretation of
the source recorder artifacts.
"""

REQUIRED_SESSION_FILES = ("capture.mp4", "sync.csv", "actions.bin")

BUTTON_NAME_TO_INDEX = {
    "south": 1,
    "east": 2,
    "west": 3,
    "north": 4,
    "back": 5,
    "guide": 6,
    "start": 7,
    "left_stick": 8,
    "right_stick": 9,
    "left_shoulder": 10,
    "right_shoulder": 11,
    "dpad_up": 12,
    "dpad_down": 13,
    "dpad_left": 14,
    "dpad_right": 15,
    "misc1": 16,
    "right_paddle1": 17,
    "left_paddle1": 18,
    "right_paddle2": 19,
    "left_paddle2": 20,
    "touchpad": 21,
}

AXIS_NAME_TO_INDEX = {
    "leftx": 0,
    "lefty": 1,
    "rightx": 2,
    "righty": 3,
    "left_trigger": 4,
    "right_trigger": 5,
}

STICK_NAME_TO_AXES = {
    "left_stick": ("leftx", "lefty"),
    "right_stick": ("rightx", "righty"),
}
