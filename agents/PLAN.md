# Plan

- [X] video capture with precise frame time recovery
    - alternative: add into moonlight/sunshine source code
        - guaranteed to have time-aligned gamepad state and video frames

- [X] test gamepad recording



- [ ] implement post-processing pipeline
    - [X] ROI annotation
        - annotation of regions: experience, health, mana, damage, stats
            - see QuPath -> load GeoJson ROIs in Python
    - [ ] ROI HUD state extraction
        - design filters
        - visualize results based on filters
    - [ ] episode done detection

    - data schema:
        - RLlib: see experience schema: https://docs.ray.io/en/latest/rllib/rllib-offline.html#using-external-expert-experiences and [SingleAgentEpisode](https://docs.ray.io/en/latest/rllib/package_ref/env/env/ray.rllib.env.single_agent_episode.SingleAgentEpisode.html#ray.rllib.env.single_agent_episode.SingleAgentEpisode)
            - easiest might be to construct `SingleAgentEpisode` following example on docs page and then serialize using built-in method?
            - or use the tabular format that conforms to the schema
        - Farama::Minari: https://minari.farama.org/content/dataset_standards/
        - [TorchRL.data](https://pytorch.org/rl/main/reference/data.html)
            - very well documented and good example in [DINO-WM repo](https://github.com/gaoyuezhou/dino_wm/blob/main/datasets/traj_dset.py)

    - temporal and spatial downsampling
    - encode using pretrained autoencoder

    - [ ] update `exp2transitions` script
        - parse both gamepad and video frames simultaneously
        - see RLLib experience format: https://docs.ray.io/en/latest/rllib/rllib-offline.html#using-external-expert-experiences
            - easiest might be to construct `SingleAgentEpisode` following [example](https://docs.ray.io/en/latest/rllib/package_ref/env/env/ray.rllib.env.single_agent_episode.SingleAgentEpisode.html#ray.rllib.env.single_agent_episode.SingleAgentEpisode) and then serialize using built-in method?

        - group sets of gamepad inputs together into (S,A,R,S) tuples
            - identify good replay buffer framework

- [ ] real-time inference
    - see notes below
    - preferably not using deepstream: see open tabs on iphone/ipad (Paulbridger posts, frame buffer extraction)
    - idea: gstream `dx12` elements: screencapture, videoconvert, followed by use of `cudaImportExternalMemory` call to convert to torch array on GPU?
        - HOWEVER: don't overoptimize, try with gstreamer pipeline that converts to system memory first

- [ ] implement game metadata file
    - gamepad -> action mapping
    - level info
    - graphics info
    - game version info
    - class/character info
    - weapon/loadout info

- [ ] Usability/UX improvements
    - launch script for video + gamepad recording

