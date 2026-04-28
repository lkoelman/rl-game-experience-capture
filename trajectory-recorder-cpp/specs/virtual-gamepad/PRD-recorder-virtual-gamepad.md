When recording the gamepad using GamepadLogger and SDL3, the gamepad events do not reach my video game. The solution is to create a virtual game


## Problem: Gamepad Logging

When recording the gamepad using GamepadLogger (see attached C++ code) and SDL3, the gamepad events do not reach my video game. It looks like reading the gamepad events consumes them so that other video games cannot use the gamepad inputs simultaneously. Can I work around this? If yes, how should I change the architecture to ensure that recorded gamepad events reach my video game?


## Solution

The issue you are experiencing—where reading gamepad events "consumes" them and prevents your game from seeing them—almost certainly depends on whether your `GamepadLogger` is running inside the **same process** (integrated into your game's code) or as a **separate background application**.


The `GamepadLogger` lives in a separate standalone application. If your logger is an entirely separate executable running alongside the game, standard Windows XInput/DirectInput normally allows multiple applications to read the same controller simultaneously. However, if your game requests exclusive access, or if you are specifically trying to build an architecture that intercepts inputs to modify or log them transparently without the game knowing, you will need the **Virtual Gamepad Proxy Architecture**.

This aligns exactly with the libraries you linked. Here is how you should design that architecture:

#### The Proxy Architecture
To achieve this, your logger must act as a "Man-in-the-Middle" between the physical hardware and the game.

1. **Hide the Physical Controller:** You must use a kernel-level filter driver like **HidHide** (standard in the Windows emulation community). You configure HidHide to make your physical gamepad invisible to all applications *except* your `GamepadLogger`.
2. **Read the Inputs:** Your `GamepadLogger` (which is whitelisted by HidHide) reads the physical gamepad via SDL3.
3. **Emit Virtual Inputs:** Your `GamepadLogger` translates the SDL3 state and feeds it into a **Virtual Gamepad Library**.
4. **The Game Plays:** The video game sees only the Virtual Gamepad (since the physical one is hidden) and reads the inputs normally.

#### Evaluating your Library Options

* **ViGEmBus (Nefarius):** This was the gold standard for years. It creates pixel-perfect virtual Xbox 360 or DualShock 4 controllers. While the original repository is archived and EOL, it is still wildly successful and stable on Windows 10/11. There are widely available forks, and the successor project (Nefarius Virtual Pad) is currently in development. If you want instant compatibility with 99% of PC games (which expect XInput), this is still the most reliable API to target.
* **WinUHid:** A modern, excellent alternative. Instead of creating a specific Xbox/DS4 controller, it allows you to inject raw HID (Human Interface Device) reports. This is great if you want to emulate a generic DirectInput controller or a custom flight stick, but requires a bit more boilerplate to structure the HID descriptors compared to ViGEmBus.
* **vJoy:** This is an older standard for creating generic DirectInput virtual joysticks. It is highly flexible but lacks native XInput (Xbox controller) emulation out of the box. Because most modern PC games rely heavily on XInput, using vJoy often requires users to run an additional layer (like x360ce) to convert the vJoy device into an Xbox controller for the game.

**Recommendation:** If you go the separate-process route, use **ViGEmBus** (or its client libraries) to emulate an Xbox 360 controller, and instruct your users to install **HidHide** to prevent the game from seeing double inputs.

*(Note: If you run your logger as a separate process, you also need to set the SDL hint `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` to `1` before initializing SDL, otherwise SDL will stop reading controller inputs when your logger window loses focus!)*


## Proof of Concept

The recommended solution using ViGEmBus was developed into a PoC in the form of the `virtual_gamepad_bridge` sample application. Its implementation plan is described in specs\virtual-gamepad\PLAN-gamepad-forwarding-example.md .

## Next Steps

The PoC has validated our solution. This has been tested to work while running a video game. The next step is to modify the GamepadLogger used by the `record_session` executable to use the same gamepad forwarding strategy. We should read and save the physical gamepad using SDL3 (already implemented), but emit virtual inputs using ViGEmBus. Read the implementation of the virtual gamepad bridge and use a similar approach.