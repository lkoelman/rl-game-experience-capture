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


## Install Gstreamer Python on Windows

Instructions adapted from https://fluendo.com/blog/gstreamer-python-bindings-for-windows/ .
Test with gstreamer commmit `df7a8c8c406740d335dddcff1e6f4b58447df5e5`.

Download `pkg-config` 'https://gstreamer.freedesktop.org/src/mirror/pkg-config.zip'
or use script in `<repository>\gstreamer\subprojects\win-pkgconfig\download-binary.py`.

```powershell
# Install meson build system (must use pip) and correct setuptools version
python -m pip install meson "setuptools==68"

# path to downloaded pkg-config.exe binary
$env:PKG_CONFIG="${pwd}\subprojects\win-pkgconfig\pkg-config.exe"

# Donload and build gstreamer
git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git
cd gstreamer
meson setup build -Dintrospection=enabled --prefix c:/Users/lucas/.local/opt/gstreamer
meson compile -C .\build
meson install -C .\build
```

Ensure gst python module is on PYTHONPATH:

```powershell
$env:PYTHONPATH="C:/Users/lucas/.local/opt/gstreamer/lib/site-packages"
```


```python
import os
import sys
from pathlib import Path

gst_install_dir = Path('C:/Users/lucas/.local/opt/gstreamer')
gst_bin_dir = gst_install_dir / 'bin'

# only if gst python module not on path:
# sys.path.append(str(gst_install_dir / 'lib' / 'site-packages'))
os.environ['PATH'] = str(gst_bin_dir)
os.add_dll_directory(gst_bin_dir)

import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst
Gst.init(sys.argv)
print(Gst.version_string())
```