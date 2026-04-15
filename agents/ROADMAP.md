

- [ ] convert gamepad actions (low-level) to player actions (high level, in-game skills/abilities)
    - just map instantaneous button state to ability state
        - use threshold for trigger axes

- [ ] LeRobotDataset converter
    - convert our recording sessions to LeRobotDataset format for easy training with PyTorch and HuggingFace libraries

- [ ] (Optional) Reward modeling for RL
    - can skip this if we start with Imitation Learning / Behavioural Cloning
    - look at reward model in Recap paper: combine time to completion / alive time with game state / HUD metrics (damage in/out, HP, mana, experience)

- [ ] Train baseline model
    - training script for simple baseline model

- [ ] Tran advanced models