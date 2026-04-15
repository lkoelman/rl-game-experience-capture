Add a tool for mapping the recorded gamepad inputs (low-level inputs) to in-game player actions (high-level inputs, in-game skills/abilities/UI actions).

Context: this tool will help us convert recorded (state, action) trajectories to a format that can be used for supervised learning or reinforcement learning. Because there are multiple possible input devices (gamepads, mouse & keyboard) and multiple in-game classes with distinct abilities, the action space should represent in-game actions rather than raw gamepad inputs.

The tool should consist of the following components:

1. A yaml configuration file that captures the video game properties that are useful in our recording and training pipeline.
    - it lists in-game actions, skills, and abilities that can be activated using the gamepad or input device
    - these actions should be grouped by section (this can be a character class or specialization)


2. An interactive button mapping tool.
    - its goal is to record the mapping between gamepad inputs and in-game actions
    - UX: it uses a TUI to guide the user through the mapping process
        - first it prompts for the character class and/or specializations
        - then it will walk through each in-game action read from the yaml config file (corresponding to the selected class and specializations): for each action it will ask the user to press the keybindings on the gamepad
        - the TUI should accept keyboard inputs for "skip/next action" ('n' key) and "confirm" (spacebar)
    - it should produce a new config file "action-mapping.yaml" that maps the gamepad inputs to in-game actions