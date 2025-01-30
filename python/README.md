### Usage

```bash
# Example: Let the script auto-detect creation_time from metadata
python frame_reader.py path/to/video.mp4

# Example: Specify an absolute creation_time in ISO8601
python frame_reader.py path/to/video.mp4 --creation_time "2025-01-30T12:00:00"
```

### Notes and Tips

- **ISO 8601 Format**:  
  By default, this script attempts to parse creation times in standard ISO 8601 (e.g., `2023-01-05T12:34:56`). If you have fractional seconds or a trailing `Z`, it does a minimal transformation to handle common formats. For more robust parsing, you can replace `datetime.fromisoformat()` with something like:
  
  ```python
  from dateutil import parser
  ...
  absolute_start_dt = parser.parse(creation_time_str)
  ```
  That requires `pip install python-dateutil`.

- **Extracting Frames**:  
  If you need to process the raw pixel data, you can call:
  ```python
  img = frame.to_ndarray(format='bgr24')
  ```
  inside the loop.

- **Variable Frame Rate**:  
  PyAV ensures you get the *actual* presentation times from the container. This script works for both constant and variable frame rate videos.

- **No Absolute Timestamp Needed?**  
  If all you need is a relative timeline (i.e., how many seconds into the video each frame occurs), you can ignore the `--creation_time` argument entirely. The script will print relative timestamps by default.
