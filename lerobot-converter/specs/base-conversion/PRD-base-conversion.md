Build a conversion tool that converts recorded gameplay sessions, together with an action mapping profile written by `ActionMappingWorkflow.cpp`, to the LeRobotDataset format.

First read trajectory-recorder-cpp/README.md and trajectory-recorder-cpp/docs/ARCHITECTURE.md to understand the architecture of the game session recording and action mapping tools.

The LeRobotDataset source code is cloned locally in ~/workspace/lerobot . The key file in the repo is `src/lerobot/datasets/lerobot_dataset.py` containing the LeRobotDataset class implementation. The repo contains example scripts that illustrate how to use the `LeRobotDataset` class:
- crop_dataset_roi.py
- port_droid.py
- test_dataset_tools.py