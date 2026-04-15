
# Model Training

- train model
    - [ ] DINO-WM
        - create a dataloader that uses `decord` together with the video, frame metadata, and gamepad recording csv
        - set the config correctly
        - try training


## Integrate with DINO-WM Repository

@workspace I want to train a visual world model based on a custom dataset. Please explain what parts of the code and config I should modify.

Based on the code, I'll explain how to adapt it for your custom dataset. You'll need to make changes in several areas:

First, create a new dataset class similar to `PointMazeDataset` by subclassing `TrajDataset`:

```python
class CustomDataset(TrajDataset):
    def __init__(
        self,
        data_path: str = "data/custom",
        n_rollout: Optional[int] = None,
        transform: Optional[Callable] = None,
        normalize_action: bool = False,
        action_scale=1.0,
    ):
        # Required data structures:
        self.states = # Your state data (shape: n_rollout x timesteps x state_dim)
        self.actions = # Your action data (shape: n_rollout x timesteps x action_dim)
        self.proprios = # Your proprioception data (can be same as states)
        self.seq_lengths = # Length of each trajectory
```

2. Create a data loading function similar to the ones in the codebase:

```python
def load_custom_slice_train_val(
    transform,
    n_rollout=50,
    data_path='data/custom',
    normalize_action=False,
    split_ratio=0.8,
    num_hist=0,
    num_pred=0,
    frameskip=0,
):
    dset = CustomDataset(
        n_rollout=n_rollout,
        transform=transform,
        data_path=data_path,
        normalize_action=normalize_action,
    )
    # Use existing train/val split function
    dset_train, dset_val, train_slices, val_slices = get_train_val_sliced(
        traj_dataset=dset,
        train_fraction=split_ratio,
        num_frames=num_hist + num_pred,
        frameskip=frameskip
    )
```

3. Create a new environment config file in conf/env/custom.yaml:

```yaml
name: custom_env
args: []
kwargs: {}
dataset:
  _target_: "datasets.custom_dset.load_custom_slice_train_val"
  n_rollout: null
  normalize_action: ${normalize_action}
  data_path: ${oc.env:DATASET_DIR}/custom
  split_ratio: 0.9
  transform:
    _target_: "datasets.img_transforms.default_transform"
    img_size: ${img_size}
decoder_path: null
num_workers: 16
```

4. Ensure your dataset follows this structure:

- States: Contains state information for each timestep
- Actions: Contains action taken at each timestep
- Images: Visual observations (RGB images) for each timestep
- Sequence lengths: Length of each trajectory

5. To train, use:

```sh
export DATASET_DIR=/path/to/your/data
python train.py --config-name train.yaml env=custom frameskip=5 num_hist=3
```

Key considerations:
- Your dataset should include visual observations (images), actions, and states
- Images should be RGB format and consistent size
- Actions and states should be normalized (handled by the dataset class)
- Sequence lengths should be tracked for each trajectory
- The data dimensions should match what's expected by the world model (check visual_world_model.py for expected dimensions)

Q: What do the states represent?

Based on the code, I'll explain what states represent in the PointMaze dataset:

1. States Structure:
- The states are loaded from states.pth as floating point tensors
- They represent the agent's position in the point maze environment
- Structure: [n_rollouts, timesteps, state_dim]

2. State Properties:
- Used as proprioceptive information (copied to self.proprios)
- Normalized if normalize_action=True
- Stored alongside corresponding actions and sequence lengths
- State dimensionality stored in self.state_dim

Here's the relevant code that handles the states:

```python
states = torch.load(self.data_path / "states.pth").float()
self.states = states
...
self.state_dim = self.states.shape[-1]
...
# States are used as proprioceptive info
self.proprios = self.states.clone()
```

In a point maze environment, states typically represent:
- x,y coordinates of the agent's position
- Additional dimensions could include velocities or orientation
- Each timestep captures the agent's full state information

The exact state dimensions would be visible in the shape of the loaded tensor.

Q: So they represent sensory data or other observations/information about the world state that is not visual

