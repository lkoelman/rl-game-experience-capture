Read the documentation for the d3d11screencapturesrc element used in our video recorder: https://gstreamer.freedesktop.org/documentation/d3d11/d3d11screencapturesrc.html?gi-language=c . It takes a monitor index, monitor handle, and window handle. With the current behaviour, it seems like the default monitor and window are recorded. I want to add a feature that lets the user select the correct monitor or window before starting the recording.

Requirements and constraints:
- This should be done using the https://github.com/ArthurSonzogni/FTXUI TUI library.
- The TUI should list the windows by title, and list the monitors together with information to identify them (hardware model, resolution, ... whatever else is easily avaible using Windows libraries).
- It should be easy to navigate the TUI menu using keyboard keys (for example: press 0 or 1 first for fullscreen / window, then press 1-9 for a window or <space> to show additional windows)
- It should be possible the bypass the monitor/window selection menu by specifiying command line argument (e.g. --monitor <id> or --window <title>)