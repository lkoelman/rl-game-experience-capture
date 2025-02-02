# Plan

- [ ] video capture with precise frame time recovery
    - A: ffmpeg
        - 
    - B: gstreamer-python
        - set up gstreamer pipeline with `gst-python` and add a callback when new frame is received
    - alternative: add into moonlight/sunshine source code
        - guaranteed to have time-aligned gamepad state and video frames
- [ ] define annotation pipeline
    - launch script for video + gamepad recording
        - cannot be both interactive
        - can just use windows gamebar (windows+G)
    - or just use the windows gamebar and use the file's `CTIME`
- [ ] test gamepad recording
- [ ] implement post-processing pipeline
    - data schema:
        - RLlib: see experience schema: https://docs.ray.io/en/latest/rllib/rllib-offline.html#using-external-expert-experiences and [SingleAgentEpisode](https://docs.ray.io/en/latest/rllib/package_ref/env/env/ray.rllib.env.single_agent_episode.SingleAgentEpisode.html#ray.rllib.env.single_agent_episode.SingleAgentEpisode)
        - Farama::Minari: https://minari.farama.org/content/dataset_standards/
    - annotation of regions: experience, health, mana, damage, stats
    - episode done detection
    - temporal and spatial downsampling
    - encode using pretrained autoencoder
- [ ] update `exp2transitions` script
    - parse both gamepad and video frames simultaneously
    - see RLLib experience format: https://docs.ray.io/en/latest/rllib/rllib-offline.html#using-external-expert-experiences
        - easiest might be to construct `SingleAgentEpisode` following [example](https://docs.ray.io/en/latest/rllib/package_ref/env/env/ray.rllib.env.single_agent_episode.SingleAgentEpisode.html#ray.rllib.env.single_agent_episode.SingleAgentEpisode) and then serialize using built-in method?
        - or use the tabular format that conforms to the scehma
    - group sets of gamepad inputs together into (S,A,R,S) tuples
        - identify good replay buffer framework
- [ ] implement game metadata file
    - gamepad -> action mapping
    - level info
    - graphics info
    - game version info
    - class/character info
    - weapon/loadout info


## Refactoring

- [ ] precise frame times without drift
    - currently the first frame time has a delay w.r.t. the file creation time
    - this can be solved by writing the system timestamp when new frames become available
        - using a callback in GStreamer or ffmpeg

- [ ] should we switch from XInput API to Windows.Gaming.Input API?
    * good summary of different gamepad APIs: https://github.com/MysteriousJ/Joystick-Input-Examples?tab=readme-ov-file
    * [Windows\.Gaming\.Input Namespace \- Windows apps \| Microsoft Learn](https://learn.microsoft.com/en-us/uwp/api/windows.gaming.input?view=winrt-26100 "Windows.Gaming.Input Namespace - Windows apps | Microsoft Learn")
    * [Gamepad Class \(Windows\.Gaming\.Input\) \- Windows apps \| Microsoft Learn](https://learn.microsoft.com/en-us/uwp/api/windows.gaming.input.gamepad?view=winrt-26100 "Gamepad Class \(Windows.Gaming.Input\) - Windows apps | Microsoft Learn")
    * [Microsoft\'s new GameInput API is going to open up so many possibilities \: r\/PeripheralDesign](https://www.reddit.com/r/PeripheralDesign/comments/1b259di/microsofts_new_gameinput_api_is_going_to_open_up/ "Microsoft\'s new GameInput API is going to open up so many possibilities : r/PeripheralDesign")


- SDL2 notes
    - game controllers
        * [SDL2\/APIByCategory \- SDL2 Wiki](https://wiki.libsdl.org/SDL2/APIByCategory "SDL2/APIByCategory - SDL2 Wiki")
        * [Beginner\-friendly demonstration using SDL2 to read from a joystick\.](https://gist.github.com/fabiocolacio/6af2ef76a706443adb210d23bd036d04 "Beginner-friendly demonstration using SDL2 to read from a joystick.")
        * [A demonstration of using SDL2 to poll the state of buttons and axes on a joystick](https://gist.github.com/fabiocolacio/423169234b8daf876d8eb75d8a5f2e95 "A demonstration of using SDL2 to poll the state of buttons and axes on a joystick")
        * [SDL\_GameController\: Making gamepads just work – rubenwardy\'s blog](https://blog.rubenwardy.com/2023/01/24/using_sdl_gamecontroller/ "SDL_GameController: Making gamepads just work – rubenwardy\'s blog")
    - building and project layout
        * [main\.cpp · main · rubenwardy \/ sdl\_gamecontroller\_example · GitLab](https://gitlab.com/rubenwardy/sdl_gamecontroller_example/-/blob/main/main.cpp?ref_type=heads "main.cpp · main · rubenwardy / sdl_gamecontroller_example · GitLab")
        * [\[SDL2\.x\] WinMain vs\. main \- SDL Development \- Simple Directmedia Layer](https://discourse.libsdl.org/t/sdl2-x-winmain-vs-main/51334/4 "[SDL2.x] WinMain vs. main - SDL Development - Simple Directmedia Layer")
        * [Getting started with the SDL2 library for Game Development](https://blog.conan.io/2023/07/20/introduction-to-game-dev-with-sdl2.html "Getting started with the SDL2 library for Game Development")
