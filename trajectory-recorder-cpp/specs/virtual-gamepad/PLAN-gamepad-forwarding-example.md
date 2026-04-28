 # ViGEmClient Meson Integration and Windows Forwarder Sample

  ## Summary

  Integrate thirdparty/ViGEmClient as an in-tree Meson-built static dependency, without removing or modifying the existing CMake files beyond leaving them unused. Add a new Windows-only console sample
  executable that reads one physical SDL gamepad and forwards its state to a ViGEm virtual Xbox 360 controller. Do not change GamepadLogger yet.

  This sample is for integration validation only: build/link correctness, driver connectivity, SDL event capture, and end-to-end forwarding through a virtual pad visible to games.

  ## Implementation Changes

  - Add thirdparty/ViGEmClient/meson.build that mirrors the current CMake library target as a static library.
      - Build src/ViGEmClient.cpp and src/ViGEmClient.rc.
      - Export a Meson dependency that exposes include/ publicly and src/ privately.
      - Link setupapi.
      - Keep examples disabled for now.
      - Do not delete or rewrite thirdparty/ViGEmClient/CMakeLists.txt; Meson becomes our build path, CMake remains present.
  - Update the root meson.build to consume the new submodule dependency.
      - Add subdir('thirdparty/ViGEmClient').
      - Define a vigemclient_dep from the submodule Meson target.
      - Add a new Windows-only executable target for the sample, separate from record_session.
      - Do not link ViGEm into core_lib or GamepadLogger yet; keep the integration isolated behind the sample.
  - Add a new sample executable in src/, as a minimal CLI.
      - Suggested target name: virtual_gamepad_bridge.
      - Responsibilities:
          - initialize SDL with gamepad events enabled and background input allowed
          - connect to ViGEmBus via vigem_alloc / vigem_connect
          - create one Xbox 360 virtual target
          - forward changes to the virtual controller on event updates
      - Scope limits:
          - no shared abstraction extraction unless needed to keep the sample readable
          - no rumble or LED callback support in v1
          - new executable name and purpose
          - requirement that ViGEmBus be installed
          - requirement that physical controller hiding is handled externally via HidHide or equivalent
          - brief run instructions for the sample
      - Update docs/ARCHITECTURE.md to mention the new validation path and the new Meson-managed third-party dependency.

  ## Public Interfaces / Behavior

    - New build target:
      - virtual_gamepad_bridge
  - New Meson subproject interface:
      - vigemclient_dep exposed from thirdparty/ViGEmClient/meson.build
  - Sample runtime behavior:
      - starts a virtual Xbox 360 controller
      - opens the first SDL-detected gamepad
      - forwards live button/axis changes to the virtual controller
      - prints concise lifecycle/status messages only
      - assumes the physical controller is already hidden from the game by external tooling

  ## Test Plan

  - Follow TDD for the new deterministic translation logic.
      - First add failing tests for SDL-state-to-XUSB_REPORT mapping.
      - Cover:
          - neutral state maps to zeroed report
          - face buttons map to expected XUSB flags
          - trigger values map to byte range
          - thumbstick axes preserve sign and expected range
          - unchanged input does not require a new report payload decision helper, if one is factored
  - Keep hardware-dependent behavior out of unit tests.
      - No automated test should require ViGEmBus or a physical controller.
      - The sample executable itself is validated manually.
  - Manual validation on Windows:
      1. Build with scripts/build.ps1.
      2. Run virtual_gamepad_bridge.
      3. Confirm ViGEm connection succeeds and one virtual Xbox 360 controller appears.
      4. With HidHide configured, confirm the game sees only the virtual controller.
      5. Move sticks / press buttons on the physical controller and confirm the virtual controller mirrors them in a tester or target game.
      6. Confirm clean shutdown removes the virtual device.

  - thirdparty/ViGEmClient already contains the intended source snapshot and remains a git submodule.
  - The first milestone uses a static build only; DLL output and install/export packaging from the old CMake flow are intentionally ignored in Meson.
› Implement the plan.
