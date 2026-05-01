# No-Reencode Conversion Plan

## Summary

Add a --no-reencode conversion path beside the current frame-decode/re-encode path. The default conversion remains unchanged except that --max-pre-action-seconds becomes optional. When
--max-pre-action-seconds is omitted, no leading frames are trimmed, equivalent to unlimited pre-action context.

With --no-reencode and no explicit --max-pre-action-seconds, the converter should reuse the original capture.mp4 directly in the LeRobot video layout without re-encoding or trimming.
If trimming is requested, it should attempt stream-copy remux trimming without re-encoding.

Chosen approach: a converter-owned adapter around LeRobotDataset.create() and DatasetWriter, not subclassing or monkey-patching LeRobot. This is most maintainable because all private
LeRobot coupling is isolated in one module, the original path remains intact, and future LeRobot changes have one narrow integration point to update.

## Key Changes

- Add CLI flag --no-reencode.
- Change --max-pre-action-seconds from required to optional.
- Extend convert_sessions(..., max_pre_action_seconds: float | None, no_reencode: bool = False).
- Interpret max_pre_action_seconds=None as “do not trim any pre-action frames.”
- Keep the current re-encoding method as the default.
- Add a sibling no-reencode method in the pipeline:
    - current path: _write_session_episode(...)
    - new path: _write_session_episode_no_reencode(...)
- Persist converter metadata for:
    - selected video mode: no_reencode
    - optional trim setting: max_pre_action_seconds as null when omitted

Implementation shape:

- Add a converter-owned helper module, e.g. game2lerobot/no_reencode.py, containing:
    - NoReencodeEpisodeWriter: builds non-video episode buffers, saves parquet data, writes episode metadata, and delegates only the needed LeRobot writer internals.
    - copy_original_video(...): places the original MP4 into LeRobot’s expected video layout when no trimming is requested.
    - remux_trimmed_video_segment(...): creates a trimmed MP4 segment using stream copy when trimming is requested.
    - compute_video_observation_stats(...): decodes retained frames for accurate observation stats.

No-reencode data flow:

1. Read session artifacts as today.
2. If max_pre_action_seconds is None, retain all frame indices.
3. Otherwise, apply the existing pre-action trim logic.
4. Decode the first retained frame to create/validate the LeRobot feature schema.
5. Build non-video episode rows with the same action/timestamp/task semantics as today.
6. Compute accurate stats from retained frames, including video observation stats.
7. If no trimming was requested, copy or move the original capture.mp4 into LeRobot’s video location without remuxing.
8. If trimming was requested, remux a trimmed segment using stream copy and set LeRobot from_timestamp according to any keyframe preroll.
9. Save episode parquet, video metadata, stats, tasks, and final dataset metadata through the adapter.

Trimming policy:

- Omitted --max-pre-action-seconds: never trim; no-reencode may use the original MP4.
- Explicit --max-pre-action-seconds: attempt no-reencode trimming/remuxing.
- If requested trimming cannot be represented safely without re-encoding, skip the session in non-strict mode or fail in strict mode with a clear reason.
- Keyframe preroll is allowed for stream-copy trimming, but must be reflected in from_timestamp.

## Test Plan

- CLI parser test: --max-pre-action-seconds may be omitted.
- CLI parser test: --no-reencode is accepted and passed into convert_sessions.
- Unit test: omitted max_pre_action_seconds retains all frames.
- Regression test: default conversion still uses the existing add_frame() / save_episode() path.
- Unit test: no-reencode without trimming does not call LeRobotDataset.add_frame() and does not call LeRobot video encoding.
- Unit test: no-reencode without trimming places the original MP4 bytes or stream-equivalent file into the dataset video path.
- Integration-style test: no-reencode with omitted trim can decode expected frames through LeRobotDataset.
- Integration-style test: no-reencode with explicit trim creates a shorter stream-copy segment when possible and sets correct video timestamps.
- Stats test: observation stats are computed from retained frames.
- Strict-mode test: unsafe no-reencode trim fails in strict mode and skips in non-strict mode.
- Run uv run pytest, then ruff check/format on touched files during implementation.

## Assumptions

- Omitted --max-pre-action-seconds means no trimming, not the old required behavior.
- Explicit --max-pre-action-seconds 0 still means keep no pre-action context before the first action.
- With --no-reencode and omitted trim, using the original MP4 is intended and preferred.
- Accurate observation stats require decoding retained frames, but decoding for stats is acceptable because the video is not re-encoded.
- Documentation will be updated in README.md and architecture docs to describe --no-reencode, optional trimming, and compatibility constraints.
