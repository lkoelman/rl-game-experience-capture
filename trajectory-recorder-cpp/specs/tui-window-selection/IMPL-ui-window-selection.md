Implemented the monitor/window selection feature. `record_session` now accepts `--monitor <id>` and `--window <title>`, otherwise it opens an FTXUI selector before GStreamer/session startup. The resolved target is passed through `Session` into `VideoRecorder`, and the pipeline now sets `monitor-handle`/`monitor-index` or `window-handle` accordingly. The new deterministic selection layer and startup UI live in `include/CaptureSelection.hpp`, `src/CaptureSelection.cpp`, `include/CaptureSelector.hpp`, and `src/CaptureSelector.cpp`. CLI parsing and recorder wiring changed in `include/RecordCli.hpp`, `src/main_record.cpp`, `include/Session.hpp`, and `include/VideoRecorderPipeline.hpp`.

I added test coverage for the new deterministic behavior in `tests/RecordCliTests.cpp`, `tests/VideoRecorderPathTests.cpp`, and `tests/CaptureSelectionTests.cpp`. Build integration now includes `ftxui/6.1.9` in `conanfile.txt` and Meson wiring in `meson.build`. Docs were updated in `README.md` and `docs/ARCHITECTURE.md`.

Verification:

- Ran `powershell -ExecutionPolicy Bypass -File scripts/build.ps1`
- All 4 tests passed
- Smoke-tested the real binary with an invalid monitor id:
  - `record_session.exe --monitor 9999`
  - It exited with `Error while selecting the capture target: No monitor matches id 9999.`

Residual risk: I verified the non-interactive startup path and the deterministic selector logic, but I did not do a live manual pass through the interactive FTXUI menu against real windows/monitors.
