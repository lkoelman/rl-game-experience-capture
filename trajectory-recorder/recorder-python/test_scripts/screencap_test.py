import os
import sys
from pathlib import Path

# System-specific gstreamer install path
gst_install_dir = Path('C:/Users/lucas/.local/opt/gstreamer')
gst_bin_dir = gst_install_dir / 'bin'

# only if gst python module not on path:
sys.path.append(str(gst_install_dir / 'lib' / 'site-packages'))

os.environ['PATH'] = str(gst_bin_dir)
os.add_dll_directory(gst_bin_dir)

import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject, GLib

# Initialize GStreamer
Gst.init(sys.argv)
print(Gst.version_string())

frame_number = 0

def frame_probe(pad, info, _user_data):
    global frame_number

    gst_buffer = info.get_buffer()
    if not gst_buffer:
        print("Unable to get GstBuffer ")
        return

    # Intiallizing object counter with 0.
    print(f"frame {frame_number}")
    frame_number += 1

    return Gst.PadProbeReturn.OK



if __name__ == '__main__':
    # Create Pipeline
    pipeline = Gst.Pipeline()

    # Create elements
    src = Gst.ElementFactory.make('d3d11screencapturesrc', 'my_screencap')
    sink = Gst.ElementFactory.make('autovideosink', 'my_videosink')

    # Set monitor-index property to 1
    src.set_property('monitor-index', 1)

    # Check if elements were created successfully
    if not src or not sink:
        print("Failed to create elements")
        sys.exit(1)

    # Add elements to pipeline
    pipeline.add(src)
    pipeline.add(sink)

    # Link elements
    if not src.link(sink):
        print("Failed to link elements")
        sys.exit(1)

    # Add probe
    srcpad = src.get_static_pad('src')
    srcpad.add_probe(Gst.PadProbeType.BUFFER, frame_probe, 0)

    # Set pipeline state to playing
    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("Failed to set pipeline to playing state")
        sys.exit(1)

    # Create main loop
    loop = GLib.MainLoop()

    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        # Clean up
        pipeline.set_state(Gst.State.NULL)