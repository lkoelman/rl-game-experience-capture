"""
Convert captured video game experiences to transition that can
be stored in a replay buffer.
"""

import argparse
import av
from datetime import datetime, timedelta

def main():
    parser = argparse.ArgumentParser(
        description="Read an MP4 file frame by frame, printing precise timestamps."
    )
    parser.add_argument("mp4_file", help="Path to the MP4 file.")
    parser.add_argument(
        "--creation_time",
        help=(
            "Absolute timestamp of the first frame in ISO 8601 format "
            "(e.g. '2023-01-05T12:34:56'). If not provided, the script "
            "will attempt to read 'creation_time' from the MP4 metadata."
        ),
        default=None,
    )

    args = parser.parse_args()

    # Open the MP4 container with PyAV.
    container = av.open(args.mp4_file)

    # Attempt to get the creation_time from command line or from metadata
    creation_time_str = args.creation_time
    if creation_time_str is None:
        # MP4 containers often store a 'creation_time' in their metadata (in ISO format).
        creation_time_str = container.format.metadata.get("creation_time", None)

    # Try to parse the creation_time string into a datetime object
    absolute_start_dt = None
    if creation_time_str:
        # Some metadata strings might have a trailing 'Z' or fractional seconds.
        # We'll do a simple replace for 'Z' and try datetime.fromisoformat.
        # For more robust parsing, consider using 'dateutil.parser.parse'.
        try:
            # fromisoformat does not handle a trailing 'Z' (UTC marker), so remove it if present:
            creation_time_str = creation_time_str.replace("Z", "")
            absolute_start_dt = datetime.fromisoformat(creation_time_str)
        except ValueError:
            # If parsing fails, we'll just warn and proceed without absolute times
            print(f"Warning: Could not parse creation_time '{creation_time_str}' as ISO 8601. Ignoring.")
            absolute_start_dt = None

    # Select the first video stream
    video_stream = container.streams.video[0]

    # Loop over packets in the container, then decode each packet to get frames
    for packet in container.demux(video_stream):
        for frame in packet.decode():
            # Some frames may not have an established PTS yet (very early frames)
            if frame.pts is None:
                continue

            # Time base is usually a Fraction. Multiply it by pts to get presentation time in seconds.
            frame_timestamp_sec = float(frame.pts * frame.time_base)

            # If we have an absolute creation time, compute the absolute frame time
            if absolute_start_dt:
                frame_abs_time = absolute_start_dt + timedelta(seconds=frame_timestamp_sec)

                # TODO: add to replay buffer together with actions and rewards
                print(
                    f"Frame {frame.index} - "
                    f"Relative: {frame_timestamp_sec:.4f} s - "
                    f"Absolute: {frame_abs_time.isoformat()}"
                )
            else:
                print(f"Frame {frame.index} - Relative: {frame_timestamp_sec:.4f} s")

if __name__ == "__main__":
    main()