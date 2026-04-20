# LeGamer

<p align="center">
  <img src="docs/assets/huggingface-logo.svg" alt="Hugging Face" height="72" style="vertical-align: middle;" />
  <span style="display: inline-block; margin: 0 16px; font-size: 32px; font-weight: 700; line-height: 72px; vertical-align: middle;">X</span>
  <img src="docs/assets/path-of-exile_PoE2_icon.png" alt="Path of Exile 2" height="72" style="vertical-align: middle;" />
</p>


Record your gaming sessions. Turn it into training data. Build agents that learn how you play.

LeGamer is a monorepo for game-centric agent training workflows: capture synchronized gameplay sessions, map raw controller input to meaningful in-game actions, convert recordings into `LeRobotDataset` format, and prepare for a future where custom agents can assist you, imitate your style, and eventually compete against each other.

The core idea is simple:

1. Record a game session with synchronized video and controller input.
2. Translate low-level button presses into high-level game actions like "move", "roll", or "use skill".
3. Export those sessions into a training-friendly dataset.
4. Train agents that can learn from your play and improve over time.

## Why LeGamer

This repo is aimed at training policies to play real AAA games with a minimal, user-friendly setup, without privileged state informaton.

- build your own gameplay dataset from the games you actually play
- train agents that learn your controls, your timing, and your habits
- create assistants that can help with combat, movement, farming, or repeated tasks
- experiment with imitation learning, reward modeling and RL
- move toward agents that can spar, race, or compete against each other in the same game ecosystem

Today, the repo already supports the first half of that loop well: recording sessions, validating them, defining action vocabularies, and converting data into a format that plugs into the wider LeRobot, PyTorch, and Hugging Face training stack.

## What Works Today

### `trajectory-recorder-cpp`

The C++ recorder is the data-capture backbone of the repo.

It currently supports:

- recording gameplay into synchronized artifacts:
  - `capture.mp4`
  - `sync.csv`
  - `actions.bin`
- selecting a monitor or specific window before recording
- live SDL3 gamepad input capture
- offline validation of recorded sessions
- interactive mapping from raw gamepad controls to high-level in-game actions
- YAML-based game definitions and per-user action-mapping profiles

This is the package that lets you say: "I want this trigger pull to mean `heavy_attack`, and this stick input to mean `move`."

See [trajectory-recorder-cpp/README.md](./trajectory-recorder-cpp/README.md) and [trajectory-recorder-cpp/docs/ARCHITECTURE.md](./trajectory-recorder-cpp/docs/ARCHITECTURE.md).

### `lerobot-converter`

The Python converter takes recorded sessions and turns them into a single `LeRobotDataset`.

It currently supports:

- discovering recorded session directories in a batch
- validating required session files
- aligning video frames to the latest input snapshot
- trimming leading idle footage before the first action
- encoding high-level mapped actions into a dense float action vector
- exporting one episode per retained session
- writing converter metadata into the dataset

This is the bridge from raw gameplay capture to model training.

See [lerobot-converter/README.md](./lerobot-converter/README.md) and [lerobot-converter/docs/ARCHITECTURE.md](./lerobot-converter/docs/ARCHITECTURE.md).

## Current Workflow

If you want to use the repo today, the practical loop looks like this:

1. Define a game's action vocabulary in YAML.
2. Use `map_actions` to create an action-mapping profile for your controller.
3. Record gameplay sessions with `record_session`.
4. Inspect the outputs with `validate_recording`.
5. Convert batches of sessions with `game2lerobot`.
6. Train downstream models with the exported LeRobot dataset.

Example converter command:

```bash
cd lerobot-converter
uv run game2lerobot \
  --session-root ../recordings \
  --game-definition ../trajectory-recorder-cpp/configs/path-of-exile-2-game-definition.yaml \
  --action-mapping ../trajectory-recorder-cpp/configs/action-mapping-example.yaml \
  --output-root ./output/path-of-exile-2 \
  --repo-id local/path_of_exile_2 \
  --task "Clear the zone" \
  --max-pre-action-seconds 1.0
```

Example recorder tools:

```powershell
.\builddir\map_actions.exe .\configs\path-of-exile-2-game-definition.yaml
.\builddir\record_session.exe .\data poe2_session_01 --window "Path of Exile 2"
.\builddir\validate_recording.exe .\data --json
```

## What’s Coming Next

The roadmap in [agents/ROADMAP.md](./agents/ROADMAP.md) points toward a full game-agent training stack.

Planned next steps include:

- improving and documenting the LeRobot conversion workflow with example recordings and mappings
- training a baseline model on captured gameplay
- exploring stronger model families for video action modeling
- adding reward modeling for RL-focused training
- inferring actions directly from video so the pipeline can scale beyond fully instrumented recordings

That means the repo is not just about logging gameplay. It is about building toward agents that can:

- learn to play from demonstration
- adapt to your preferred control scheme and action vocabulary
- operate as helpers or co-pilots during play
- be evaluated against each other in repeatable training environments

## Repo Layout

- [trajectory-recorder-cpp](./trajectory-recorder-cpp/) Windows-first C++ capture, validation, and action-mapping tools
- [lerobot-converter](./lerobot-converter/) Python converter from recorded sessions to `LeRobotDataset`

## Status

This repo is already useful if you want to capture gameplay trajectories and prepare them for imitation-learning style experiments.

It is not yet the complete end-to-end "train a strong game agent in one command" experience. The training stack is still emerging. But the key foundation is here already: synchronized recording, action semantics, dataset conversion, and a clear roadmap toward custom game-playing agents.
