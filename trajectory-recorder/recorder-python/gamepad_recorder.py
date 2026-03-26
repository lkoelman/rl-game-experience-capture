"""
Records gamepad state to CSV files using SDL3.
"""

import time
import datetime
import csv
from pathlib import Path
import sdl3

# Enum values from https://wiki.libsdl.org/SDL3/SDL_GamepadButton
# FIXME: can we get this from the enum?
SDL_GAMEPAD_BUTTONS = (
    sdl3.SDL_GAMEPAD_BUTTON_INVALID,
    sdl3.SDL_GAMEPAD_BUTTON_SOUTH,           # Bottom face button (e.g. Xbox A button) */
    sdl3.SDL_GAMEPAD_BUTTON_EAST,            # Right face button (e.g. Xbox B button) */
    sdl3.SDL_GAMEPAD_BUTTON_WEST,            # Left face button (e.g. Xbox X button) */
    sdl3.SDL_GAMEPAD_BUTTON_NORTH,           # Top face button (e.g. Xbox Y button) */
    sdl3.SDL_GAMEPAD_BUTTON_BACK,
    sdl3.SDL_GAMEPAD_BUTTON_GUIDE,
    sdl3.SDL_GAMEPAD_BUTTON_START,
    sdl3.SDL_GAMEPAD_BUTTON_LEFT_STICK,
    sdl3.SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    sdl3.SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    sdl3.SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    sdl3.SDL_GAMEPAD_BUTTON_DPAD_UP,
    sdl3.SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    sdl3.SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    sdl3.SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    sdl3.SDL_GAMEPAD_BUTTON_MISC1,           # Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia capture sdl3.button) */
    sdl3.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,   # Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1) */
    sdl3.SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,    # Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3) */
    sdl3.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,   # Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2) */
    sdl3.SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,    # Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4) */
    sdl3.SDL_GAMEPAD_BUTTON_TOUCHPAD,        # PS4/PS5 touchpad button */
    sdl3.SDL_GAMEPAD_BUTTON_MISC2,           # Additional button */
    sdl3.SDL_GAMEPAD_BUTTON_MISC3,           # Additional button */
    sdl3.SDL_GAMEPAD_BUTTON_MISC4,           # Additional button */
    sdl3.SDL_GAMEPAD_BUTTON_MISC5,           # Additional button */
    sdl3.SDL_GAMEPAD_BUTTON_MISC6,           # Additional button */
    sdl3.SDL_GAMEPAD_BUTTON_COUNT
)

class SdlGamepadRecorder:
    """
    Records the gamepad state for all connected gamepads.
    Saves input data to CSV files with timestamps.
    """

    def __init__(self, output_file: Path):
        """Initialize SDL and prepare for gamepad recording
        """
        if not sdl3.SDL_Init(sdl3.SDL_INIT_GAMEPAD | sdl3.SDL_INIT_EVENTS):
            raise RuntimeError(f"Failed to initialize SDL: {sdl3.SDL_GetError().decode().lower()}")

        # Allow background events
        sdl3.SDL_SetHint(sdl3.SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, b"1")
        
        self._gamepad = None
        self._csv_file = None
        self._csv_writer = None
        self._recording = False

        # Try to find and open first available gamepad
        # FIXME: we could fetch multiple gamepad by doing sdl3.SDL_GetGamepad(MAX_COUNT); foreach: sdl3.SDL_OpenGamepad(...)
        gamepad_ids = sdl3.SDL_GetGamepads(None)
        if not gamepad_ids:
            print("No gamepads detected!")
            return False
        else:
            # print(f"Found {len(list(gamepad_ids))} gamepads. Using fist one.")
            print("Found gamepad")

        self._joystick_id = gamepad_ids[0]
        self._gamepad = sdl3.SDL_OpenGamepad(self._joystick_id)
        if not self._gamepad:
            print(f"Failed to open gamepad: {sdl3.SDL_GetError().decode().lower()}")
            return False
        
        # Find out what buttons this controller has
        self._valid_buttons = (
            button for button in SDL_GAMEPAD_BUTTONS
            if sdl3.SDL_GamepadHasButton(self._gamepad, button)
        )

        try:
            self._csv_file = open(output_file, 'w', newline='')
            self._csv_writer = csv.writer(self._csv_file)
            
            # Write CSV header
            self._csv_writer.writerow([
                "timestamp",
                "controller_type",
                "buttons",
                "left_stick_x", 
                "left_stick_y",
                "right_stick_x",
                "right_stick_y",
                "left_trigger",
                "right_trigger",
            ])
            
            self._recording = True

        except IOError as e:
            print(f"Failed to open output file: {e}")
            if self._gamepad:
                sdl3.SDL_CloseGamepad(self._gamepad)
                self._gamepad = None

    def stop_recording(self):
        """Stop recording and close files
        """
        if not self._recording:
            return

        self._recording = False
        
        if self._csv_file:
            self._csv_file.close()
            self._csv_file = None
            self._csv_writer = None

        if self._gamepad:
            sdl3.SDL_CloseGamepad(self._gamepad)
            self._gamepad = None

        sdl3.SDL_Quit()


    def record_unhandled_events(self):
        """Record gamepad state when buttons are (de)pressed or joysticks moved.
        """
        if not self._recording or not self._gamepad:
            return

        # Process SDL events to keep controller state updated
        num_events = 0  # events handles
        event = sdl3.SDL_Event()
        while sdl3.SDL_PollEvent(event):

            match event.type:
                # See event types: https://wiki.libsdl.org/SDL3/SDL_Event
                case sdl3.SDL_EVENT_GAMEPAD_REMOVED:
                    if self._gamepad and event.gdevice.which == sdl3.SDL_GetGamepadID(self._gamepad):
                        self.stop_recording()
                        return
                    
                case sdl3.SDL_EVENT_GAMEPAD_BUTTON_DOWN | sdl3.SDL_EVENT_GAMEPAD_BUTTON_UP:
                    # TODO: if we open multiple gamepads we can just save the joystick_id
                    button_event = event.gbutton  # SDL_GamepadButtonEvent
                    if self._joystick_id != button_event.which:
                        print('wrong gamepad event')
                        continue
                    timestamp = button_event.timestamp # ns
                
                case sdl3.SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    axis_event = event.gaxis  # SDL_GamepadAxisEvent
                    if self._joystick_id != axis_event.which:
                        print('wrong gamepad axis event')
                        continue
                    timestamp = axis_event.timestamp
                case _:
                    continue

            # Get controller type
            controller_type = sdl3.SDL_GetGamepadType(self._gamepad)

            # Get button states (combined into bit flags)
            button_mask = 0
            for button in self._valid_buttons:
                if sdl3.SDL_GetGamepadButton(self._gamepad, button):
                    button_mask |= (1 << button)

            # Get analog inputs
            left_x = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFTX)
            left_y = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFTY)
            right_x = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHTX)
            right_y = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHTY)
            left_trigger = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            right_trigger = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)

            # Write state to CSV
            self._csv_writer.writerow([
                timestamp,
                controller_type,
                button_mask,
                left_x,
                left_y, 
                right_x,
                right_y,
                left_trigger,
                right_trigger,
            ])

            num_events += 1

        if num_events > 0:
            print(f"Handled {num_events} events.")


    def record_gamepad_state(self):
        """Save current gamepad state regardless of whether it was updated
        (button/axis events received).
        """
        if not self._recording or not self._gamepad:
            return

        # Process SDL events to keep controller state updated
        event = sdl3.SDL_Event()
        while sdl3.SDL_PollEvent(event):
            match event.type:
                case sdl3.SDL_EVENT_GAMEPAD_REMOVED:
                    if self._gamepad and event.gdevice.which == sdl3.SDL_GetGamepadID(self._gamepad):
                        return

        # Get current timestamp
        timestamp = int(time.time() * 1000)

        # Get controller type
        controller_type = sdl3.SDL_GetGamepadType(self._gamepad)

        # Get button states (combined into bit flags)
        button_mask = 0
        for button in self._valid_buttons:
            if sdl3.SDL_GetGamepadButton(self._gamepad, button):
                button_mask |= (1 << button)

        # Get analog inputs
        left_x = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFTX)
        left_y = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFTY)
        right_x = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHTX)
        right_y = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHTY)
        left_trigger = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
        right_trigger = sdl3.SDL_GetGamepadAxis(self._gamepad, sdl3.SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)

        # Write state to CSV
        self._csv_writer.writerow([
            timestamp,
            controller_type,
            button_mask,
            left_x,
            left_y, 
            right_x,
            right_y,
            left_trigger,
            right_trigger,
        ])


if __name__ == '__main__':
    dt = f"{datetime.datetime.now():%y-%m-%dT%H%M%S}"
    recorder = SdlGamepadRecorder(f"gamepad_recording_{dt}.csv")

    try:
        # TODO: can do at slower rate and handle more events
        while True:
            recorder.record_unhandled_events()
            time.sleep(0.010)
    except KeyboardInterrupt:
        print("Shutting down")
    finally:
        recorder.stop_recording()