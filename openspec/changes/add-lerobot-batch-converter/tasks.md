## 1. Converter CLI and batch session discovery

- [x] 1.1 Create the batch-oriented converter CLI in `lerobot-converter` with arguments for session-root, game-definition, action-mapping profile, output root, repo id, task text, maximum pre-action duration, and `--strict`.
- [x] 1.2 Implement batch session discovery and per-session required-file validation for `capture.mp4`, `sync.csv`, and `actions.bin`.
- [x] 1.3 Implement best-effort batch behavior by default and strict-failure behavior behind `--strict`.

## 2. Session parsing and frame alignment

- [x] 2.1 Implement readers for `sync.csv` and `actions.bin` that produce ordered frame timestamps and raw gamepad snapshots.
- [x] 2.2 Implement video-frame iteration that preserves recorded frame order and original session cadence.
- [x] 2.3 Implement last-known-state alignment for each retained frame and enforce the configurable maximum pre-action duration for leading frames.
- [x] 2.4 Define and validate the pre-first-action baseline state used for retained leading frames.

## 3. High-level action encoding

- [x] 3.1 Implement ordered action collection from the selected `class_ids` in the loaded game definition.
- [x] 3.2 Implement binding evaluation from aligned raw gamepad state into high-level action values for `digital`, `analog`, `trigger`, and `vector2` actions.
- [x] 3.3 Implement dense `action` vector layout generation with stable slot ordering and zero-filled unmapped actions.
- [x] 3.4 Exclude keyboard state from v1 action derivation and add validation or documentation for unsupported keyboard-driven mappings.

## 4. LeRobot dataset writing and metadata persistence

- [x] 4.1 Define the v1 LeRobot feature schema with one video observation feature and one dense `action` feature.
- [x] 4.2 Create the LeRobot dataset writer flow that writes one converted session as one episode and finalizes the dataset after batch completion.
- [x] 4.3 Add a namespaced converter metadata block to `meta/info.json` containing source game, mapping identity, conversion settings, action layout, retained sessions, and skipped-session failures.
- [x] 4.4 Ensure dataset FPS is preserved from the recorded source and validate or reject incompatible session cadence during batch conversion.

## 5. Verification and examples

- [x] 5.1 Add tests for session validation, strict versus non-strict batch handling, and leading-frame trimming behavior.
- [x] 5.2 Add tests for dense action vector layout generation, per-kind encoding semantics, and zero-filling of unmapped actions.
- [x] 5.3 Add tests that verify converter metadata is written into `meta/info.json` under the namespaced extension block.
- [x] 5.4 Add a documented example command using `trajectory-recorder-cpp/configs/path-of-exile-2-game-definition.yaml` and `trajectory-recorder-cpp/configs/action-mapping-example.yaml`.
