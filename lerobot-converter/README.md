# Game2LeRobot

Video game (state, action) trajectory to LeRobotDataset converter.

## Usage

Run the batch converter from the `lerobot-converter` directory:

```bash
uv run game2lerobot \
  --session-root ../recordings \
  --game-definition ../trajectory-recorder-cpp/configs/path-of-exile-2-game-definition.yaml \
  --action-mapping ../trajectory-recorder-cpp/configs/action-mapping-example.yaml \
  --output-root ./output/path-of-exile-2 \
  --repo-id local/path_of_exile_2 \
  --task "Clear the zone" \
  --max-pre-action-seconds 1.0
```

Add `--strict` to fail the run when any discovered session directory is invalid instead of skipping it.
