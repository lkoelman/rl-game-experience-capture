"""
Convert captured video game experiences to transition that can
be stored in a replay buffer.
"""

import argparse
import av
import os
from datetime import datetime, timedelta

def main():
    parser = argparse.ArgumentParser(
        description="Read an MP4 file frame by frame, printing precise timestamps."
    )
    parser.add_argument("mp4_file", help="Path to the MP4 file.")
    parser.add_argument(
        "--start-time",
        help=(
            "How to determine the start time for the first frame. Can be: \n"
            "'metadata' - read from MP4 metadata (default)\n"
            "'ctime' - use file creation time\n"
            "ISO 8601 timestamp (e.g. '2023-01-05T12:34:56')"
        ),
        default='ctime'
    )

    args = parser.parse_args()

    # Open the MP4 container with PyAV
    container = av.open(args.mp4_file)

    # Get start time based on the specified method
    absolute_start_dt = None
    
    if args.start_time == 'metadata':
        # Try to get creation time from MP4 metadata
        creation_time_str = container.format.metadata.get("creation_time", None)
        if creation_time_str:
            try:
                # Remove trailing 'Z' if present
                creation_time_str = creation_time_str.replace("Z", "")
                absolute_start_dt = datetime.fromisoformat(creation_time_str)
            except ValueError:
                print(f"Warning: Could not parse metadata creation_time '{creation_time_str}' as ISO 8601.")
                
    elif args.start_time == 'ctime':
        # Use file creation time
        ctime = os.path.getctime(args.mp4_file)
        absolute_start_dt = datetime.fromtimestamp(ctime)
        
    else:
        # Try to parse as ISO 8601 timestamp
        try:
            absolute_start_dt = datetime.fromisoformat(args.start_time)
        except ValueError:
            print(f"Warning: Could not parse '{args.start_time}' as ISO 8601 timestamp.")

    if absolute_start_dt is None:
        raise ValueError("No valid start time found.")

    # Select the first video stream
    video_stream = container.streams.video[0]
    frame_index = 0

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
                    f"Frame {frame_index} - "
                    f"Relative: {frame_timestamp_sec:.4f} s - "
                    f"Absolute: {frame_abs_time.isoformat()} ({frame_abs_time.timestamp()})"
                )
            else:
                print(f"Frame {frame.index} - Relative: {frame_timestamp_sec:.4f} s")

            frame_index += 1

if __name__ == "__main__":
    main()