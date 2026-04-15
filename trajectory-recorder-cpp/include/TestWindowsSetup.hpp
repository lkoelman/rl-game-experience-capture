#pragma once

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#include <windows.h>
#endif

namespace trajectory::test_support {

// Prevent **Microsoft C Runtime (CRT)** and **Windows Error Reporting (WER)** GUI pop-ups.
// When you build using the MSVC toolchain the CRT is designed to intercept fatal errors—like `assert()` failures, unhandled exceptions, or calls to `abort()`—and display a graphical "Debug Error!" dialog box. Similarly, if the process outright crashes (e.g., a segmentation fault), Windows may trigger a crash dialog asking if you want to debug or close the program.
// => tell the Windows API and the MSVC CRT to route errors to `stderr` and fail silently at the OS level, rather than spawning a GUI.
inline void DisableWindowsErrorDialogs() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    _set_abort_behavior(0, _WRITE_ABORT_MSG);

    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
}

}  // namespace trajectory::test_support
