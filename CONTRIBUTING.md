# Plan

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
