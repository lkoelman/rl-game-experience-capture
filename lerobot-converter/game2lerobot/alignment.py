"""Frame and session alignment helpers.

These functions bridge recorder-oriented artifacts and the frame-oriented
LeRobot writer by deciding which session folders are convertible and which
video frames survive the leading pre-action trim.
"""

from pathlib import Path

from .constants import REQUIRED_SESSION_FILES
from .models import SessionValidationResult


def collect_session_dirs(root: Path) -> list[Path]:
    """Return child directories that are treated as candidate recorded sessions."""

    return sorted(path for path in root.iterdir() if path.is_dir())


def validate_session_dir(session_dir: Path) -> SessionValidationResult:
    """Check that a discovered session contains the recorder artifact triplet."""

    missing_files = tuple(
        sorted(
            name for name in REQUIRED_SESSION_FILES if not (session_dir / name).exists()
        )
    )
    return SessionValidationResult(ok=not missing_files, missing_files=missing_files)


def trim_idle_frame_indices(
    frame_timestamps_ns: list[int],
    first_action_timestamp_ns: int | None,
    max_pre_action_seconds: float,
) -> list[int]:
    """Trim the leading idle region while preserving the original frame cadence.

    The converter uses the video stream as the authoritative timeline. This
    helper keeps all frames after the first action and only the configured
    amount of context before it.
    """

    if first_action_timestamp_ns is None:
        return list(range(len(frame_timestamps_ns)))

    max_pre_action_ns = int(max_pre_action_seconds * 1_000_000_000)
    retained: list[int] = []
    for index, timestamp_ns in enumerate(frame_timestamps_ns):
        if (
            timestamp_ns < first_action_timestamp_ns
            and first_action_timestamp_ns - timestamp_ns > max_pre_action_ns
        ):
            continue
        retained.append(index)
    return retained
