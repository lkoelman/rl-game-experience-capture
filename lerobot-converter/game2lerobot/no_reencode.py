"""No-reencode LeRobot episode writer.

This module intentionally contains the converter's private LeRobot writer
coupling so the default conversion path can stay on the public
``LeRobotDataset.add_frame`` API.
"""

from __future__ import annotations

import logging
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from lerobot.datasets.compute_stats import (
    auto_downsample_height_width,
    compute_episode_stats,
    get_feature_stats,
    sample_indices,
)
from lerobot.datasets.lerobot_dataset import LeRobotDataset

from .action_encoding import encode_action_vector
from .models import ActionLayoutEntry, FrameSyncRow, GamepadSnapshot

logger = logging.getLogger(__name__)


VIDEO_KEY = "observation.images.main"


def save_episode_without_reencoding(
    *,
    session_name: str,
    dataset: LeRobotDataset,
    source_video_path: Path,
    video_reader,
    sync_rows: list[FrameSyncRow],
    retained_indices: list[int],
    snapshots: list[GamepadSnapshot],
    bindings_by_action,
    layout: list[ActionLayoutEntry],
    task: str,
) -> None:
    """Save one session as a LeRobot episode while preserving encoded video packets."""

    writer = dataset.writer
    episode_buffer = writer._create_episode_buffer()
    baseline = GamepadSnapshot(
        monotonic_ns=0, axes=(), pressed_buttons=(), pressed_keys=()
    )
    snapshot_index = 0
    current_snapshot = baseline

    for frame_position, source_frame_index in enumerate(retained_indices):
        frame_timestamp = sync_rows[source_frame_index].monotonic_ns
        while (
            snapshot_index < len(snapshots)
            and snapshots[snapshot_index].monotonic_ns <= frame_timestamp
        ):
            current_snapshot = snapshots[snapshot_index]
            snapshot_index += 1

        episode_buffer["frame_index"].append(frame_position)
        episode_buffer["timestamp"].append(frame_position / dataset.meta.fps)
        episode_buffer["task"].append(task)
        episode_buffer[VIDEO_KEY].append(None)
        episode_buffer["action"].append(
            encode_action_vector(layout, bindings_by_action, current_snapshot)
        )
        episode_buffer["size"] += 1

    logger.info(
        "Writing session %s episode with %d frame(s) without video re-encoding",
        session_name,
        episode_buffer["size"],
    )
    _save_prepared_episode(
        dataset=dataset,
        episode_buffer=episode_buffer,
        source_video_path=source_video_path,
        video_reader=video_reader,
        sync_rows=sync_rows,
        retained_indices=retained_indices,
    )


def _save_prepared_episode(
    *,
    dataset: LeRobotDataset,
    episode_buffer: dict,
    source_video_path: Path,
    video_reader,
    sync_rows: list[FrameSyncRow],
    retained_indices: list[int],
) -> None:
    """Persist a prebuilt no-reencode episode through LeRobot writer internals.

    This mirrors the parts of `DatasetWriter.save_episode()` that handle
    non-video parquet rows, tasks, stats, video metadata, and metadata updates,
    while replacing the encoder step with a copied or stream-copied MP4.
    """

    writer = dataset.writer
    meta = dataset.meta
    episode_length = episode_buffer.pop("size")
    tasks = episode_buffer.pop("task")
    episode_tasks = list(set(tasks))
    episode_index = episode_buffer["episode_index"]

    episode_buffer["index"] = np.arange(
        meta.total_frames, meta.total_frames + episode_length
    )
    episode_buffer["episode_index"] = np.full((episode_length,), episode_index)
    meta.save_episode_tasks(episode_tasks)
    episode_buffer["task_index"] = np.array(
        [meta.get_task_index(task) for task in tasks]
    )

    for key, feature in meta.features.items():
        if key in ["index", "episode_index", "task_index"] or feature["dtype"] in [
            "image",
            "video",
        ]:
            continue
        episode_buffer[key] = np.stack(episode_buffer[key])

    non_video_buffer = {
        key: value
        for key, value in episode_buffer.items()
        if meta.features.get(key, {}).get("dtype") != "video"
    }
    non_video_features = {
        key: value for key, value in meta.features.items() if value["dtype"] != "video"
    }
    episode_stats = compute_episode_stats(non_video_buffer, non_video_features)
    episode_stats[VIDEO_KEY] = compute_video_observation_stats(
        video_reader=video_reader, retained_indices=retained_indices
    )

    episode_metadata = writer._save_episode_data(episode_buffer)
    temp_video_path = materialize_episode_video(
        source_video_path=source_video_path,
        dataset_root=dataset.root,
        sync_rows=sync_rows,
        retained_indices=retained_indices,
        fps=meta.fps,
    )
    episode_metadata.update(
        writer._save_episode_video(
            VIDEO_KEY,
            episode_index,
            temp_path=temp_video_path,
        )
    )
    meta.save_episode(
        episode_index, episode_length, episode_tasks, episode_stats, episode_metadata
    )
    writer.episode_buffer = writer._create_episode_buffer()


def compute_video_observation_stats(
    *, video_reader, retained_indices: list[int]
) -> dict:
    """Compute LeRobot-compatible image stats from retained video frames."""

    sampled_indices = sample_indices(len(retained_indices))
    images = None
    for output_index, retained_position in enumerate(sampled_indices):
        frame = _frame_to_numpy(video_reader[retained_indices[retained_position]])
        frame = np.moveaxis(frame, 2, 0)
        frame = auto_downsample_height_width(frame)
        if images is None:
            images = np.empty((len(sampled_indices), *frame.shape), dtype=np.uint8)
        images[output_index] = frame

    stats = get_feature_stats(images, axis=(0, 2, 3), keepdims=True)
    return {
        key: value if key == "count" else np.squeeze(value / 255.0, axis=0)
        for key, value in stats.items()
    }


def materialize_episode_video(
    *,
    source_video_path: Path,
    dataset_root: Path,
    sync_rows: list[FrameSyncRow],
    retained_indices: list[int],
    fps: int,
) -> Path:
    """Create a temporary MP4 for LeRobot to move into its video layout."""

    temp_dir = Path(tempfile.mkdtemp(dir=dataset_root))
    temp_path = temp_dir / source_video_path.name
    keeps_full_video = retained_indices == list(range(len(sync_rows)))
    if keeps_full_video:
        shutil.copy2(source_video_path, temp_path)
        return temp_path

    remux_trimmed_video_segment(
        source_video_path=source_video_path,
        output_path=temp_path,
        start_s=sync_rows[retained_indices[0]].pts_ns / 1_000_000_000,
        duration_s=_retained_duration_s(sync_rows, retained_indices, fps),
    )
    return temp_path


def remux_trimmed_video_segment(
    *,
    source_video_path: Path,
    output_path: Path,
    start_s: float,
    duration_s: float,
) -> None:
    """Trim an MP4 with stream copy; no decoded frames are encoded."""

    command = [
        _ffmpeg_executable(),
        "-hide_banner",
        "-loglevel",
        "error",
        "-ss",
        f"{start_s:.9f}",
        "-i",
        str(source_video_path),
        "-t",
        f"{duration_s:.9f}",
        "-map",
        "0:v:0",
        "-an",
        "-c:v",
        "copy",
        "-avoid_negative_ts",
        "make_zero",
        "-y",
        str(output_path),
    ]
    try:
        subprocess.run(command, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as exc:
        message = exc.stderr.strip() or exc.stdout.strip() or str(exc)
        raise RuntimeError(
            f"failed to remux source video without re-encoding: {message}"
        ) from exc


def _retained_duration_s(
    sync_rows: list[FrameSyncRow], retained_indices: list[int], fps: int
) -> float:
    """Return the source-video duration covered by retained frame indices.

    The end timestamp is the next frame's PTS when available; for the last
    source frame, one frame interval is added from the dataset FPS.
    """

    last_retained_index = retained_indices[-1]
    start_ns = sync_rows[retained_indices[0]].pts_ns
    if last_retained_index + 1 < len(sync_rows):
        end_ns = sync_rows[last_retained_index + 1].pts_ns
    else:
        end_ns = sync_rows[last_retained_index].pts_ns + int(1_000_000_000 / fps)
    return max((end_ns - start_ns) / 1_000_000_000, 1 / fps)


def _ffmpeg_executable() -> str:
    """Resolve an ffmpeg executable for stream-copy remuxing.

    Prefers the `imageio-ffmpeg` bundled binary when available and falls back
    to `ffmpeg` on PATH.
    """

    try:
        from imageio_ffmpeg import get_ffmpeg_exe

        return get_ffmpeg_exe()
    except Exception:
        return "ffmpeg"


def _frame_to_numpy(frame) -> np.ndarray:
    """Convert decord-style frames or numpy arrays into a numpy array."""

    if hasattr(frame, "asnumpy"):
        return frame.asnumpy()
    return frame
