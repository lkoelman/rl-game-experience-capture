"""Batch conversion orchestration.

The pipeline module coordinates discovered sessions, parses recorder artifacts,
aligns frames to input state, encodes actions, and materializes the final
LeRobot dataset. It is the only place that touches both source recordings and
the LeRobot writer.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from lerobot.datasets.io_utils import write_info
from lerobot.datasets.lerobot_dataset import LeRobotDataset

from .alignment import (
    collect_session_dirs,
    trim_idle_frame_indices,
    validate_session_dir,
)
from .action_encoding import (
    build_action_layout,
    collect_actions_by_class,
    encode_action_vector,
)
from .metadata import apply_converter_metadata, build_features
from .models import (
    ConversionMetadata,
    ConversionResult,
    GameDefinition,
    GamepadSnapshot,
)
from .parsing import open_video_reader, read_actions_bin, read_sync_csv


def convert_sessions(
    *,
    session_root: Path,
    game_definition: GameDefinition,
    action_mapping,
    output_root: Path,
    repo_id: str,
    task: str,
    max_pre_action_seconds: float,
    strict: bool,
) -> ConversionResult:
    """Convert a batch root of recorded sessions into one LeRobot dataset.

    Side effects:
    - Reads session artifacts from `session_root`.
    - Creates and writes a LeRobot dataset under `output_root`.
    - Persists converter metadata into `meta/info.json`.
    """

    action_definitions = collect_actions_by_class(
        game_definition.classes, action_mapping.class_ids
    )
    layout = build_action_layout(action_definitions)
    skipped_sessions: dict[str, str] = {}
    converted_sessions: list[str] = []
    dataset: LeRobotDataset | None = None
    expected_fps: int | None = None

    for session_dir in collect_session_dirs(session_root):
        validation = validate_session_dir(session_dir)
        if not validation.ok:
            reason = f"missing required files: {', '.join(validation.missing_files)}"
            if strict:
                raise ValueError(f"{session_dir.name}: {reason}")
            skipped_sessions[session_dir.name] = reason
            continue

        try:
            video_reader, fps = open_video_reader(session_dir / "capture.mp4")
            frame_timestamps_ns = read_sync_csv(session_dir / "sync.csv")
            snapshots = read_actions_bin(session_dir / "actions.bin")
            if len(video_reader) != len(frame_timestamps_ns):
                raise ValueError("frame count does not match sync.csv entries")

            expected_fps = _resolve_expected_fps(expected_fps, fps)
            retained_indices = trim_idle_frame_indices(
                frame_timestamps_ns=frame_timestamps_ns,
                first_action_timestamp_ns=snapshots[0].monotonic_ns
                if snapshots
                else None,
                max_pre_action_seconds=max_pre_action_seconds,
            )
            if not retained_indices:
                raise ValueError("no frames retained after applying pre-action limit")

            first_frame, next_frame_index = _read_frame_array(
                video_reader=video_reader,
                frame_index=retained_indices[0],
                next_frame_index=None,
            )
            if dataset is None:
                dataset = LeRobotDataset.create(
                    repo_id=repo_id,
                    fps=fps,
                    root=output_root,
                    features=build_features(layout, first_frame.shape),
                    use_videos=True,
                    vcodec="h264",
                )

            _write_session_episode(
                dataset=dataset,
                video_reader=video_reader,
                frame_timestamps_ns=frame_timestamps_ns,
                retained_indices=retained_indices,
                snapshots=snapshots,
                bindings_by_action=action_mapping.bindings_by_action,
                layout=layout,
                task=task,
                first_frame=first_frame,
                next_frame_index=next_frame_index,
            )
            converted_sessions.append(session_dir.name)
        except Exception as exc:
            if strict:
                raise
            skipped_sessions[session_dir.name] = str(exc)

    if dataset is None:
        raise ValueError("no valid sessions were converted")

    dataset.finalize()
    metadata = ConversionMetadata(
        game_id=game_definition.game_id,
        class_ids=action_mapping.class_ids,
        profile_name=action_mapping.profile_name,
        task=task,
        strict=strict,
        max_pre_action_seconds=max_pre_action_seconds,
        action_layout=tuple(layout),
        converted_sessions=tuple(converted_sessions),
        skipped_sessions=skipped_sessions,
    )
    apply_converter_metadata(dataset.meta.info, metadata)
    write_info(dataset.meta.info, dataset.root)
    return ConversionResult(
        converted_sessions=tuple(converted_sessions),
        skipped_sessions=skipped_sessions,
        dataset=dataset,
    )


def _resolve_expected_fps(expected_fps: int | None, fps: int) -> int:
    if expected_fps is None:
        return fps
    if fps != expected_fps:
        raise ValueError(f"fps {fps} does not match dataset fps {expected_fps}")
    return expected_fps


def _write_session_episode(
    *,
    dataset: LeRobotDataset,
    video_reader,
    frame_timestamps_ns: list[int],
    retained_indices: list[int],
    snapshots: list[GamepadSnapshot],
    bindings_by_action,
    layout,
    task: str,
    first_frame: np.ndarray,
    next_frame_index: int,
) -> None:
    baseline = GamepadSnapshot(
        monotonic_ns=0, axes=(), pressed_buttons=(), pressed_keys=()
    )
    snapshot_index = 0
    current_snapshot = baseline

    for retained_position, frame_index in enumerate(retained_indices):
        frame_timestamp = frame_timestamps_ns[frame_index]
        while (
            snapshot_index < len(snapshots)
            and snapshots[snapshot_index].monotonic_ns <= frame_timestamp
        ):
            current_snapshot = snapshots[snapshot_index]
            snapshot_index += 1
        if retained_position == 0:
            frame = first_frame
        else:
            frame, next_frame_index = _read_frame_array(
                video_reader=video_reader,
                frame_index=frame_index,
                next_frame_index=next_frame_index,
            )
        dataset.add_frame(
            {
                "observation.images.main": frame,
                "action": encode_action_vector(
                    layout, bindings_by_action, current_snapshot
                ),
                "task": task,
            }
        )

    dataset.save_episode()


def _read_frame_array(*, video_reader, frame_index: int, next_frame_index: int | None):
    if next_frame_index == frame_index:
        frame = video_reader.next()
    else:
        frame = video_reader[frame_index]
    return _frame_to_numpy(frame), frame_index + 1


def _frame_to_numpy(frame) -> np.ndarray:
    if hasattr(frame, "asnumpy"):
        return frame.asnumpy()
    return frame
