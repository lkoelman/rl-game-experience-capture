import os
import sys
import signal
from datetime import datetime
from pathlib import Path
import csv
from queue import Queue
from threading import Thread
import time

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

# check for available gstreamer plugins
# NOTE: can also use gst-inspect-1.0 shell command, e.g. `gst-inspect-1.0.exe | Select-String -Pattern 'enc'`
def gst_check_plugins():
    Gst.init(sys.argv)
    registry = Gst.Registry.get()
    plugins = registry.get_plugin_list()

    print("Available GStreamer plugins:")
    for plugin in plugins:
        print(f"{plugin.get_name()} - {plugin.get_description()}")


# working
# .\gst-launch-1.0.exe -v d3d12screencapturesrc ! d3d12convert ! d3d12download ! openh264enc ! h264parse ! mp4mux ! filesink location=videotestsrc2.mp4 -e

class VideoRecorder:
    
    # def __init__(self, monitor_index=1, output_csv=None, output_video=None):
    #     # Initialize GStreamer
    #     Gst.init(sys.argv)
    #     print(Gst.version_string())
        
    #     dt = f"{datetime.now():%y-%m-%dT%H%M%S}"
    #     if not output_csv:
    #         output_csv = f"frame_timestamps_{dt}.csv"
    #     if not output_video:
    #         output_video = f"video_{dt}.mp4"

    #     self.frame_number = 0
    #     self.csv_queue = Queue()
    #     self.output_csv = output_csv
    #     self.running = True
    #     self.output_video = output_video
        
    #     # Start CSV writer thread
    #     self.csv_thread = Thread(target=self._write_csv)
    #     self.csv_thread.daemon = True
    #     self.csv_thread.start()
        
    #     # Create Pipeline
    #     self.pipeline = Gst.Pipeline()

    #     # Create elements
    #     d3d12screencapture = Gst.ElementFactory.make("d3d12screencapturesrc", "screen_capture")
    #     d3d12screencapture.set_property('monitor-index', monitor_index)
    #     capsfilter = Gst.ElementFactory.make("capsfilter", "capture_capsfilter")
    #     # Set desired capture caps: D3D12Memory, 30 fps
    #     capture_caps = Gst.Caps.from_string("video/x-raw(memory:D3D12Memory),framerate=30/1")
    #     capsfilter.set_property("caps", capture_caps)
    #     d3d12convert = Gst.ElementFactory.make("d3d12convert", "convert")
    #     d3d12download = Gst.ElementFactory.make("d3d12download", "d3d12download")
    #     queue = Gst.ElementFactory.make("queue", "queue")
    #     d3d12h264enc = Gst.ElementFactory.make("d3d12h264enc", "h264_encoder")
    #     h264parse = Gst.ElementFactory.make("h264parse", "h264_parse")
    #     mp4mux = Gst.ElementFactory.make("mp4mux", "mp4_mux")
    #     filesink = Gst.ElementFactory.make("filesink", "file_sink")
    #     filesink.set_property("location", self.output_video)

    #     # Add all elements to the pipeline
    #     self.pipeline.add(d3d12screencapture)
    #     self.pipeline.add(capsfilter)
    #     self.pipeline.add(d3d12convert)
    #     self.pipeline.add(d3d12download)
    #     self.pipeline.add(queue)
    #     self.pipeline.add(d3d12h264enc)
    #     self.pipeline.add(h264parse)
    #     self.pipeline.add(mp4mux)
    #     self.pipeline.add(filesink)

    #     # Link elements in sequence
    #     elems = (d3d12screencapture, capsfilter, d3d12convert, d3d12download, queue, d3d12h264enc, h264parse, mp4mux, filesink)
    #     if not all(elems):
    #         raise RuntimeError(f"Failed to create elements.")
    #     if not Gst.Element.link_many(*elems):
    #         raise RuntimeError("Could not link elems")
        
    #     # Add probe
    #     srcpad = self.src.get_static_pad('src')
    #     srcpad.add_probe(Gst.PadProbeType.BUFFER, self._frame_probe, 0)
        
    #     # Create main loop
    #     self.loop = GLib.MainLoop()

    def __init__(self, monitor_index=1, output_csv=None, output_video=None):
        # Initialize GStreamer
        Gst.init(sys.argv)
        print(Gst.version_string())
        
        dt = f"{datetime.now():%y-%m-%dT%H%M%S}"
        if not output_csv:
            output_csv = f"frame_timestamps_{dt}.csv"
        if not output_video:
            output_video = f"video_{dt}.mp4"

        self.frame_number = 0
        self.csv_queue = Queue()
        self.output_csv = output_csv
        self.running = True
        self.output_video = output_video
        
        # Start CSV writer thread
        self.csv_thread = Thread(target=self._write_csv)
        self.csv_thread.daemon = True
        self.csv_thread.start()
        
        # Create Pipeline
        self.pipeline = Gst.Pipeline()

        # TODO: consider other codecs such as the ones included in `nvcodec`: https://gstreamer.freedesktop.org/documentation/nvcodec/index.html?gi-language=c#plugin-nvcodec
        # d3d12screencapturesrc ! d3d12convert ! d3d12download ! openh264enc ! h264parse ! mp4mux ! filesink location=videotestsrc2.mp4 -e
        
        # Create elements
        self.src = Gst.ElementFactory.make('d3d12screencapturesrc', 'my_screencap')
        self.videoconvert = Gst.ElementFactory.make('d3d12convert', 'd3d12cpnvert')
        self.download = Gst.ElementFactory.make('d3d12download', 'd3d12download')
        self.x264enc = Gst.ElementFactory.make('d3d12h264enc', 'h264enc')
        # self.x264enc = Gst.ElementFactory.make('x264enc', 'x264enc')
        # self.x264enc = Gst.ElementFactory.make('mfh264enc', 'mfh264enc')
        self.h264parse = Gst.ElementFactory.make("h264parse", "h264_parse")
        self.mp4mux = Gst.ElementFactory.make('mp4mux', 'mp4mux')
        self.filesink = Gst.ElementFactory.make('filesink', 'filesink')

        # Check if elements were created successfully
        elems = {
            'src': self.src,
            'videoconvert': self.videoconvert,
            'download': self.download,
            'h264enc': self.x264enc,
            'h264parse': self.h264parse,
            'mp4mux': self.mp4mux,
            'filesink': self.filesink
        }
        if not all(elems.values()):
            failed_elems = [k for k, v in elems.items() if not v]
            raise RuntimeError(f"Failed to create elements: {failed_elems}")
        
        # Set properties
        self.src.set_property('monitor-index', monitor_index)
        self.filesink.set_property('location', self.output_video)

        # FIXME: tune x264 encoder settings for speed/quality
        # self.x264enc.set_property('tune', 'zerolatency')
        # self.x264enc.set_property('speed-preset', 'ultrafast')
        
            
        # Add elements to pipeline
        self.pipeline.add(self.src)
        self.pipeline.add(self.videoconvert)
        self.pipeline.add(self.download)
        self.pipeline.add(self.x264enc)
        self.pipeline.add(self.h264parse)
        self.pipeline.add(self.mp4mux)
        self.pipeline.add(self.filesink)
        
        # Link elements
        # if not Gst.Element.link_many(self.src, self.videoconvert, self.download, self.x264enc, self.h264parse, self.mp4mux, self.filesink):
        #     raise RuntimeError("Failed to link elements")
        
        if not self.src.link(self.videoconvert):
            raise RuntimeError("Failed to link src to videoconvert")
        if not self.videoconvert.link(self.download):
            raise RuntimeError("Failed to link videoconvert to download")
        if not self.download.link(self.x264enc):
            raise RuntimeError("Failed to link download to x264enc")
        if not self.x264enc.link(self.h264parse):
            raise RuntimeError("Failed to link x264enc to h264parse")
        if not self.h264parse.link(self.mp4mux):
            raise RuntimeError("Failed to link h264parse to mp4mux")
        if not self.mp4mux.link(self.filesink):
            raise RuntimeError("Failed to link mp4mux to filesink")
            
        # Add probe
        srcpad = self.src.get_static_pad('src')
        srcpad.add_probe(Gst.PadProbeType.BUFFER, self._frame_probe, 0)
        
        # Create main loop
        self.loop = GLib.MainLoop()

    # def __init__(self, monitor_index=1, output_csv='frames.csv'):
    #     # Initialize GStreamer
    #     Gst.init(sys.argv)
    #     print(Gst.version_string())
        
    #     self.frame_number = 0
    #     self.csv_queue = Queue()
    #     self.output_csv = output_csv
    #     self.running = True
        
    #     # Start CSV writer thread
    #     self.csv_thread = Thread(target=self._write_csv)
    #     self.csv_thread.daemon = True
    #     self.csv_thread.start()
        
    #     # Create Pipeline
    #     self.pipeline = Gst.Pipeline()
        
    #     # Create elements
    #     self.src = Gst.ElementFactory.make('d3d11screencapturesrc', 'my_screencap')
    #     self.sink = Gst.ElementFactory.make('autovideosink', 'my_videosink')
        
    #     # Set monitor-index property
    #     self.src.set_property('monitor-index', monitor_index)
        
    #     # Check if elements were created successfully
    #     if not self.src or not self.sink:
    #         raise RuntimeError("Failed to create elements")
            
    #     # Add elements to pipeline
    #     self.pipeline.add(self.src)
    #     self.pipeline.add(self.sink)
        
    #     # Link elements
    #     if not self.src.link(self.sink):
    #         raise RuntimeError("Failed to link elements")
            
    #     # Add probe
    #     srcpad = self.src.get_static_pad('src')
    #     srcpad.add_probe(Gst.PadProbeType.BUFFER, self._frame_probe, 0)
        
    #     # Create main loop
    #     self.loop = GLib.MainLoop()

    def _frame_probe(self, pad, info, _user_data):
        gst_buffer = info.get_buffer()
        if not gst_buffer:
            print("Unable to get GstBuffer ")
            return

        timestamp = time.time()
        self.csv_queue.put((self.frame_number, timestamp))
        self.frame_number += 1

        return Gst.PadProbeReturn.OK
    
    def _write_csv(self):
        with open(self.output_csv, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['frame_number', 'timestamp'])
            
            while self.running:
                try:
                    frame_num, timestamp = self.csv_queue.get(timeout=1.0)
                    writer.writerow([frame_num, timestamp])
                    f.flush()
                except:
                    continue

    def start(self):
        
        # Add a bus watch to the pipeline
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        pipeline = self.pipeline
        loop = self.loop

        def on_bus_message(bus, message):
            """Handler for bus messages."""
            msg_type = message.type
            if msg_type == Gst.MessageType.EOS:
                print("End-of-stream")
                pipeline.set_state(Gst.State.NULL)
                loop.quit()
            elif msg_type == Gst.MessageType.ERROR:
                err, debug = message.parse_error()
                print(f"Error: {err}\nDebug: {debug}")
                pipeline.set_state(Gst.State.NULL)
                loop.quit()
            elif msg_type == Gst.MessageType.WARNING:
                err, debug = message.parse_warning()
                print(f"{err}: {debug}")

        bus.connect("message", on_bus_message)

        # Correctly end stream upon interrupt
        def handler(signum, frame):
            print("SIGINT detected. Sending EOS...")
            pipeline.send_event(Gst.Event.new_eos())

        signal.signal(signal.SIGINT, handler)

        # Set pipeline state to playing
        ret = self.pipeline.set_state(Gst.State.PLAYING)
        if ret == Gst.StateChangeReturn.FAILURE:
            raise RuntimeError("Failed to set pipeline to playing state")
            
        try:
            self.loop.run()
        finally:
            self.cleanup()

    def cleanup(self):
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)
        if self.csv_thread.is_alive():
            self.csv_thread.join()

if __name__ == '__main__':
    recorder = VideoRecorder(monitor_index=1)
    recorder.start()