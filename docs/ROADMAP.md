- [X] Record synchronized frames and gamepad inputs
    - tasks

- [X] convert gamepad actions (low-level) to player actions (high level, in-game skills/abilities)
    - just map instantaneous button state to ability state
        - use threshold for trigger axes
    - tasks
        - [X] implement gamepad-action mapping tool
        - [X] test action mapping tool and fix bugs/UX

- [ ] LeRobotDataset converter
    - convert our recording sessions to LeRobotDataset format for easy training with PyTorch and HuggingFace libraries
    - [X] write PRD with reference to the example recording and action mapping
    - [ ] record real PoE2 game session
    - [ ] do conversion and identify painpoints, fixes, enhancements

- [ ] (Optional) Reward modeling for RL
    - can skip this if we start with Imitation Learning / Behavioural Cloning
    - look at reward model in Recap paper: combine time to completion / alive time with game state / HUD metrics (damage in/out, HP, mana, experience)

- [ ] Train baseline model
    - training script for simple baseline model

- [ ] Train advanced models
    - training scripts for SOTA VLA, VAM, WM, ...

- [ ] Infer action from video
    - add data pipeline to infer game actions from video, based on https://github.com/AlmondGod/tinyworlds (code based on DeepMind Genie)