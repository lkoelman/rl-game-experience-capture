## Question: Gamepad Logging

Help me solve the following bug: when recording the gamepad using InputLogger (see attached C++ code) and SDL3, the gamepad events do not reach my video game. It looks like reading the gamepad events consumes them so that   other video games cannot use the gamepad inputs simultaneously. Can I work around this? If yes, how should I change the architecture to ensure that recorded gamepad events reach my video game?

For reference, I've identified several "virtual gamepad" libraries that may be useful:
- https://github.com/nefarius/ViGEmBus  (archived, would probably need patching)
- https://github.com/cgutman/WinUHid
- https://github.com/njz3/vJoy/

## Answer (Gemini 3 Pro)

The issue you are experiencing—where reading gamepad events "consumes" them and prevents your game from seeing them—almost certainly depends on whether your `InputLogger` is running inside the **same process** (integrated into your game's code) or as a **separate background application**.

Here is how to solve the problem for both architectural scenarios.

### Scenario A: `InputLogger` is running in the same process as your game
If you compiled this logger directly into your game or engine, the bug is in this specific line inside `PumpEventsOnce()`:
```cpp
while (SDL_PollEvent(&event)) { ... }
```
**The Root Cause:** SDL's event queue is shared across your entire application. `SDL_PollEvent` dequeues (removes) the event from the queue. If your logger runs on a background thread or runs before the game's main loop, it steals all the gamepad events. When the game engine later calls `SDL_PollEvent`, the queue is empty.

**The Fix:** Do not use `SDL_PollEvent` in the logger. Instead, use an **Event Watch**. `SDL_AddEventWatch` allows you to register a callback that inspects every event as it enters the queue *without* removing it, leaving it intact for your game's main loop to process.

**Code Changes:**
1. Remove `PumpEventsOnce()` entirely.
2. Create a static callback function that matches the `SDL_EventFilter` signature.
3. Register it in `Start()` using `SDL_AddEventWatch(MyEventWatchCallback, this);`.
4. Inside the callback, apply your `switch (event.type)` logic and write to the protobuf.

---

### Scenario B: `InputLogger` is a separate standalone application
If your logger is an entirely separate executable running alongside the game, standard Windows XInput/DirectInput normally allows multiple applications to read the same controller simultaneously. However, if your game requests exclusive access, or if you are specifically trying to build an architecture that intercepts inputs to modify or log them transparently without the game knowing, you will need the **Virtual Gamepad Proxy Architecture**.

This aligns exactly with the libraries you linked. Here is how you should design that architecture:

#### The Proxy Architecture
To achieve this, your logger must act as a "Man-in-the-Middle" between the physical hardware and the game.

1. **Hide the Physical Controller:** You must use a kernel-level filter driver like **HidHide** (standard in the Windows emulation community). You configure HidHide to make your physical gamepad invisible to all applications *except* your `InputLogger`.
2. **Read the Inputs:** Your `InputLogger` (which is whitelisted by HidHide) reads the physical gamepad via SDL3.
3. **Emit Virtual Inputs:** Your `InputLogger` translates the SDL3 state and feeds it into a **Virtual Gamepad Library**.
4. **The Game Plays:** The video game sees only the Virtual Gamepad (since the physical one is hidden) and reads the inputs normally.

#### Evaluating your Library Options

* **ViGEmBus (Nefarius):** This was the gold standard for years. It creates pixel-perfect virtual Xbox 360 or DualShock 4 controllers. While the original repository is archived and EOL, it is still wildly successful and stable on Windows 10/11. There are widely available forks, and the successor project (Nefarius Virtual Pad) is currently in development. If you want instant compatibility with 99% of PC games (which expect XInput), this is still the most reliable API to target.
* **WinUHid:** A modern, excellent alternative. Instead of creating a specific Xbox/DS4 controller, it allows you to inject raw HID (Human Interface Device) reports. This is great if you want to emulate a generic DirectInput controller or a custom flight stick, but requires a bit more boilerplate to structure the HID descriptors compared to ViGEmBus.
* **vJoy:** This is an older standard for creating generic DirectInput virtual joysticks. It is highly flexible but lacks native XInput (Xbox controller) emulation out of the box. Because most modern PC games rely heavily on XInput, using vJoy often requires users to run an additional layer (like x360ce) to convert the vJoy device into an Xbox controller for the game.

**Recommendation:** If you go the separate-process route, use **ViGEmBus** (or its client libraries) to emulate an Xbox 360 controller, and instruct your users to install **HidHide** to prevent the game from seeing double inputs.

*(Note: If you run your logger as a separate process, you also need to set the SDL hint `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` to `1` before initializing SDL, otherwise SDL will stop reading controller inputs when your logger window loses focus!)*


## Question: Project Integration

I've attached a zip file containing my C++ project layout, but with C++ source and header files removed. It shows how the build system is set up. The package lives in a git repository. Based on the project layout, and the structure of ViGemBus, make a plan to cleanly integrate ViGemBus in my project. It should be easy to build the project using the existing conan and meson build tools. We should minimize difficult additional setup and install steps. Should we use a git submodule? Should we build ViGemBus somewhere else and link it into our project using conan? Think about several options and recommend the best one.

## Answer (Gemini 3 Pro)

To cleanly integrate ViGEm into your `trajectory-recorder-cpp` project, we first need to make an important architectural distinction:

1.  **ViGEmBus** is a Windows Kernel-Mode Driver. You **cannot** compile or link this directly into your C++ application. End-users *must* install it on their system using an MSI installer (or your app must silently run the installer).
2.  **ViGEmClient** is the user-mode C/C++ SDK that your application will actually link against to talk to the ViGEmBus driver.

Since the original Nefarius ViGEm project is officially **Archived and End-of-Life**, the source code and APIs will never change. This gives us a massive advantage for integration: we don't need to worry about tracking upstream updates.

Based on your existing architecture (specifically how you handle GStreamer in `scripts/build.ps1` and `meson.build`), here are three options for integrating **ViGEmClient**, followed by a recommendation.

---

### Option 1: In-Tree Pre-built Binaries (Highly Recommended)
Because `ViGEmClient` is small, entirely static (will never receive updates), and Windows-only, the cleanest approach is to commit the compiled `.lib` and header files directly into your repository inside a `third_party/` folder.

* **Pros:** Zero extra setup for developers. No Conan recipes to write. No Git submodule initialization. It "just works" immediately when someone clones the repo.
* **Cons:** Commits binary files to Git (though they are very small, < 1MB).

**Implementation:**
1. Download the latest `ViGEmClient` release SDK.
2. Create `third_party/vigemclient/include/` and `third_party/vigemclient/lib/` in your repository.
3. Place `ViGEm/Client.h` in the include folder, and `setupapi.lib` / `ViGEmClient.lib` in the lib folder.
4. Add this to your `meson.build`:
```meson
# Add ViGEmClient dependency
vigem_inc = include_directories('third_party/vigemclient/include')
vigem_lib_dir = meson.current_source_dir() / 'third_party/vigemclient/lib'

vigem_dep = declare_dependency(
  include_directories : vigem_inc,
  dependencies : [
    cpp.find_library('ViGEmClient', dirs : vigem_lib_dir),
    cpp.find_library('setupapi') # Required Windows API for ViGEm
  ]
)

# Then add vigem_dep to your executable's dependencies
```

### Option 2: Git Submodule + Meson CMake Subproject
You can add the `ViGEmClient` GitHub repository as a Git submodule and compile it from source during your Meson build process.

* **Pros:** No binaries in your git history. Compiles natively with the rest of your code.
* **Cons:** ViGEmClient uses CMake, while you use Meson. You would need to use Meson's `cmake` module to build the subproject, which adds configuration complexity and requires developers to have CMake installed on their PATH alongside Meson.

**Implementation:**
1. Run `git submodule add https://github.com/nefarius/ViGEmClient.git subprojects/ViGEmClient`
2. In `meson.build`, use the CMake module:
```meson
cmake = import('cmake')
vigem_subproj = cmake.subproject('ViGEmClient')
vigem_dep = vigem_subproj.dependency('ViGEmClient')
```

### Option 3: Custom Conan Recipe
You could write a custom `conanfile.py` to package `ViGEmClient`, upload it to a private Artifactory/Nexus server, or have developers build it locally into their Conan cache.

* **Pros:** Keeps dependency management unified strictly under Conan.
* **Cons:** Massive overkill. `ViGEmClient` doesn't exist on ConanCenter. Writing and maintaining a custom recipe for a dead library just to avoid a `third_party` folder violates the goal of "minimizing difficult additional setup."

---

### The Verdict / Plan of Action

I strongly recommend **Option 1 (In-Tree Pre-built Binaries)**.

Looking at your `scripts/build.ps1`, you already handle GStreamer by pointing Meson to external pre-compiled libraries rather than building it from source via Conan. Treating ViGEmClient the same way (but keeping it directly in the repo since it's so small) matches your project's existing build philosophy perfectly.

**Your Action Plan:**

1.  **Documentation:** Update your `README.md` to instruct users that they **must install the ViGEmBus driver** on their Windows machine before using the software.
2.  **SDK Placement:** Download the ViGEmClient SDK, drop the headers and `.lib` files into a new `third_party/vigemclient` directory in your repo.
3.  **Build System:** Update `meson.build` with the `declare_dependency` block from Option 1.
4.  **Integration:** In your `InputLogger` or new `VirtualGamepad` C++ class, simply `#include <ViGEm/Client.h>` and start emitting inputs. You won't need to change `conanfile.txt` or `build.ps1` at all.