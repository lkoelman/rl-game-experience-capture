# Plan

- [X] video capture with precise frame time recovery
    - alternative: add into moonlight/sunshine source code
        - guaranteed to have time-aligned gamepad state and video frames

- [X] test gamepad recording

- [ ] implement post-processing pipeline
    - data schema:
        - RLlib: see experience schema: https://docs.ray.io/en/latest/rllib/rllib-offline.html#using-external-expert-experiences and [SingleAgentEpisode](https://docs.ray.io/en/latest/rllib/package_ref/env/env/ray.rllib.env.single_agent_episode.SingleAgentEpisode.html#ray.rllib.env.single_agent_episode.SingleAgentEpisode)
        - Farama::Minari: https://minari.farama.org/content/dataset_standards/
        - [TorchRL.data](https://pytorch.org/rl/main/reference/data.html)
    - annotation of regions: experience, health, mana, damage, stats
        - see QuPath -> load GeoJson ROIs in Python
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

- [ ] Usability/UX improvements
    - launch script for video + gamepad recording

## Refactoring

- [X] gamepad inputs with [SDL3 Python bindings](https://github.com/Aermoss/PySDL3)
    - see `example.py`: based on this and C++ gamepad code, design python class for capturing gamepad state

- [X] should we switch from XInput API to Windows.Gaming.Input API? => NO: use SDL3 instead
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

- Decord notes
    - decord with PyTorch bridge: https://github.com/thu-ml/controlvideo/blob/main/dataset.py


# Model Training

- train model
    - [ ] DINO-WM
        - create a dataloader that uses `decord` together with the video, frame metadata, and gamepad recording csv
        - set the config correctly
        - try training


## Integrate with DINO-WM Repository

@workspace I want to train a visual world model based on a custom dataset. Please explain what parts of the code and config I should modify.

Based on the code, I'll explain how to adapt it for your custom dataset. You'll need to make changes in several areas:

First, create a new dataset class similar to `PointMazeDataset` by subclassing `TrajDataset`:

```python
class CustomDataset(TrajDataset):
    def __init__(
        self,
        data_path: str = "data/custom",
        n_rollout: Optional[int] = None,
        transform: Optional[Callable] = None,
        normalize_action: bool = False,
        action_scale=1.0,
    ):
        # Required data structures:
        self.states = # Your state data (shape: n_rollout x timesteps x state_dim)
        self.actions = # Your action data (shape: n_rollout x timesteps x action_dim)
        self.proprios = # Your proprioception data (can be same as states)
        self.seq_lengths = # Length of each trajectory
```

2. Create a data loading function similar to the ones in the codebase:

```python
def load_custom_slice_train_val(
    transform,
    n_rollout=50,
    data_path='data/custom',
    normalize_action=False,
    split_ratio=0.8,
    num_hist=0,
    num_pred=0,
    frameskip=0,
):
    dset = CustomDataset(
        n_rollout=n_rollout,
        transform=transform,
        data_path=data_path,
        normalize_action=normalize_action,
    )
    # Use existing train/val split function
    dset_train, dset_val, train_slices, val_slices = get_train_val_sliced(
        traj_dataset=dset,
        train_fraction=split_ratio,
        num_frames=num_hist + num_pred,
        frameskip=frameskip
    )
```

3. Create a new environment config file in conf/env/custom.yaml:

```yaml
name: custom_env
args: []
kwargs: {}
dataset:
  _target_: "datasets.custom_dset.load_custom_slice_train_val"
  n_rollout: null
  normalize_action: ${normalize_action}
  data_path: ${oc.env:DATASET_DIR}/custom
  split_ratio: 0.9
  transform:
    _target_: "datasets.img_transforms.default_transform"
    img_size: ${img_size}
decoder_path: null
num_workers: 16
```

4. Ensure your dataset follows this structure:

- States: Contains state information for each timestep
- Actions: Contains action taken at each timestep
- Images: Visual observations (RGB images) for each timestep
- Sequence lengths: Length of each trajectory

5. To train, use:

```sh
export DATASET_DIR=/path/to/your/data
python train.py --config-name train.yaml env=custom frameskip=5 num_hist=3
```

Key considerations:
- Your dataset should include visual observations (images), actions, and states
- Images should be RGB format and consistent size
- Actions and states should be normalized (handled by the dataset class)
- Sequence lengths should be tracked for each trajectory
- The data dimensions should match what's expected by the world model (check visual_world_model.py for expected dimensions)

Q: What do the states represent?

Based on the code, I'll explain what states represent in the PointMaze dataset:

1. States Structure:
- The states are loaded from states.pth as floating point tensors
- They represent the agent's position in the point maze environment
- Structure: [n_rollouts, timesteps, state_dim]

2. State Properties:
- Used as proprioceptive information (copied to self.proprios)
- Normalized if normalize_action=True
- Stored alongside corresponding actions and sequence lengths
- State dimensionality stored in self.state_dim

Here's the relevant code that handles the states:

```python
states = torch.load(self.data_path / "states.pth").float()
self.states = states
...
self.state_dim = self.states.shape[-1]
...
# States are used as proprioceptive info
self.proprios = self.states.clone()
```

In a point maze environment, states typically represent:
- x,y coordinates of the agent's position
- Additional dimensions could include velocities or orientation
- Each timestep captures the agent's full state information

The exact state dimensions would be visible in the shape of the loaded tensor.

Q: So they represent sensory data or other observations/information about the world state that is not visual


# Design : Game State Extraction

(from ChatGPT o1)

You are a software architect specialising in machine learning and image processing. I have video recordings of video game playing sessions and I would like to extract the user interface state such as player health from graphical elements like health bars, shield bar, experience bar, etc. I would also like to detect whether the player has died (detection of a popup window), whether the player is navigating certain menus etc. Please design an image processing pipeline to extract these values and elements of the game state. I am using C++ : please suggest some appropriate software libraries to use.

Designing an image processing pipeline

OK, let me see. I’m tasked with creating an image processing pipeline in C++ to extract and analyze game UI elements from video recordings, using tools like OpenCV and dlib for image processing and feature detection.

Breaking it down

I'm working through several steps: extracting frames, preprocessing images, detecting and tracking UI elements, extracting data, and using libraries like OpenCV and Tesseract for text and object detection.

Below is a suggested high-level approach for designing an image processing pipeline in C++ to extract game state information (such as health, shield, XP bars, death/pop-up detection, menu navigation, etc.) from recorded video.

1. Overview of the Pipeline

    Video Frame Extraction
        Read each frame (or a subset of frames) from the video file using an appropriate video-reading library (e.g., OpenCV’s built-in video capture, FFmpeg directly, or GStreamer).
        Convert frames into a suitable color space (e.g., BGR, RGB, or even grayscale if color is not essential for certain detections).

    Preprocessing & ROI (Region of Interest) Identification
        Optionally downscale or crop frames to focus on HUD (Heads-Up Display) areas if performance is a concern.
        Apply filtering/thresholding (e.g., color thresholding if you know the health bar color is unique, or morphological operations for noise reduction).

    UI Elements Detection
        Health/Shields/XP Bars:
            If bars are fixed in position and color-coded, use color-based detection or simple template matching to find and extract the bar region.
            Measure the filled portion of the bar (e.g., by counting pixels or using bounding rectangles).
        Pop-up (Death Screen) Detection:
            Could be done with template matching if the death pop-up is consistent.
            Alternatively, train a small object detection model (e.g., YOLO, SSD) that recognizes the pop-up window.
        Menu Navigation Detection:
            Identify menu backgrounds or unique icons with template matching.
            Use text detection/OCR if the menu has readable textual cues.

    Data Extraction
        For each detected UI element (e.g., health bar), extract numerical or categorical values:
            Health bar length → scaled to a fraction (e.g., 75%).
            XP or textual values → OCR.
            Boolean states (menu open, pop-up on/off, dead/alive).
        Store these values in a data structure (e.g., a time-series array or a game state log).

    Post-processing / Aggregation
        Smooth or interpolate values over several frames (e.g., if the bar detection flickers between frames, use a small filter to stabilize readings).
        Derive higher-level events (e.g., “player died at 03:15 in the video,” “player opened inventory at 05:12,” etc.).

    Output / Integration
        Emit these extracted states to your main application logic, or store them in a database or file (CSV, JSON) for further analysis.

2. Detailed Steps & Techniques
2.1 Video Input and Frame Extraction

    OpenCV provides cv::VideoCapture for reading frames from a variety of file formats. Under the hood, OpenCV uses FFmpeg in most builds, so you get robust format support automatically.
    FFmpeg as a direct library gives you more control (like reading raw frames, handling custom codecs), but it’s more complex to use than OpenCV’s built-in capabilities.

Key C++ approach with OpenCV:

```cpp
#include <opencv2/opencv.hpp>

int main() {
    cv::VideoCapture cap("gameplay_video.mp4");
    if (!cap.isOpened()) {
        // handle error
        return -1;
    }

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame)) {
            // end of video or error
            break;
        }
        // Process the frame...
    }
    return 0;
}
```

2.2 Region of Interest Extraction & Preprocessing

    If certain HUD elements are always in a known screen region (e.g., top-left corner for health), you can crop to that region using cv::Rect.
    If color is important (e.g., health bar is always bright green), perform color thresholding in an HSV color space to isolate that color range.
    For robust detection, you might apply small morphological operations (erosion, dilation) to remove noise.

Example:

```cpp
// Suppose the health bar is at top-left corner, 200 wide, 25 high
cv::Rect healthBarROI(0, 0, 200, 25);
cv::Mat healthBarFrame = frame(healthBarROI);

// Convert to HSV for color threshold
cv::Mat hsv;
cv::cvtColor(healthBarFrame, hsv, cv::COLOR_BGR2HSV);

// Define color range for bright green
cv::Scalar lowerGreen(50, 100, 100);
cv::Scalar upperGreen(70, 255, 255);

cv::Mat mask;
cv::inRange(hsv, lowerGreen, upperGreen, mask);

// Optionally do morph operations
cv::Mat cleanMask;
cv::erode(mask, cleanMask, cv::Mat());
cv::dilate(cleanMask, cleanMask, cv::Mat());
```

2.3 UI Element Detection
2.3.1 Bars (Health, Shield, XP)

    Fixed Position Bars
        If the bars are always in the same place and orientation:
            After thresholding, count the number of white (non-zero) pixels in cleanMask.
            Compare to the total area of the bar’s bounding box to get a fill percentage.
        Alternatively, if you want the exact bounding rectangle, use cv::findContours to find the shape of the bar.

    Dynamic or Non-Fixed Position Bars
        If the UI can move or be scaled, you might need template matching (cv::matchTemplate) to locate the bar’s bounding box each time.
        Or train a small neural network-based detector (YOLOv5/v7/v8 or OpenCV’s DNN module with a custom model) to detect the bar’s bounding box in the frame.

2.3.2 Popup or Death Screen Detection

    Template Matching
        Capture a screenshot of the death screen or popup and use it as a template.
        Use cv::matchTemplate(frame, templateImage, result, cv::TM_CCOEFF_NORMED);
        Check if the maximum match score is above a threshold to declare detection.
    Object Detection Model
        If the popup window has variations or multiple states, an ML-based approach can be more robust. Train a simple classifier or detection model (e.g., YOLO) on labeled frames with/without the popup.

2.3.3 Menu Navigation Detection

    Template Matching
        Similar to pop-up detection, if the menu screen is fairly consistent.
    OCR + Text Detection
        Some menus might have distinctive text labels (e.g., “Inventory,” “Settings”).
        Detect text regions using OpenCV’s DNN text detection or a library like Tesseract.
        Recognize text and look for known keywords to confirm you’re in a specific menu.

2.4 Extracting Numeric Data & Interpreting Game State

    Bar Fill Percentage → Numeric Value
        For a horizontal bar: fillPercentage = (float)countOfFilledPixels / totalPixels;
        Convert this to the actual game number if you know the scale (e.g., 0–100 health).
    Textual Data (e.g., XP amount, level)
        Use Tesseract OCR (which has a C++ API) to read small areas of text from the screen.
        Preprocess the region for better OCR accuracy (binarization, morphological operations to enhance text).
    State Flags
        For each frame (or interval of frames), set flags like bool isDead, bool isMenuOpen, etc. based on template matching or object detection results.

2.5 Aggregation and Temporal Filtering

    If working with 60 FPS video, you might not need to analyze every single frame. You can sample at a lower rate (e.g., every 5 or 10 frames) for performance.
    Smooth out flickers with a short temporal filter: e.g., if isDead is detected in 10 consecutive frames, confirm the player is truly dead.

3. Recommended C++ Libraries and Tools

    OpenCV
        The go-to library for image processing, containing routines for filtering, morphological ops, color conversions, template matching, contour detection, etc.
        Also has a DNN module for running deep learning models (ONNX, Caffe, TensorFlow).
        Good for both basic and advanced image processing in C++.

    FFmpeg (directly or indirectly)
        Most robust solution for reading/writing video formats if you need fine-grained control.
        OpenCV internally uses FFmpeg for video I/O, so you often don’t need to integrate FFmpeg separately—unless you have specialized video needs.

    Tesseract OCR
        Open-source OCR engine with a C++ API.
        Works best on high-contrast, preprocessed text images.
        If your game uses stylized fonts, you may need custom training data, but it can handle many typical fonts out-of-the-box.

    YOLO (or other lightweight object detectors) with OpenCV DNN
        You can train a small custom object detector to robustly detect UI elements (health bar, pop-up windows, menu icons, etc.).
        Convert the trained model to a format supported by the OpenCV DNN module (ONNX is a good choice).
        This approach is more flexible and robust than template matching if your UI has variations or if certain elements move around on the screen.

    Boost / Standard C++ Libraries
        Useful for data structures, concurrency, I/O if needed. Not specifically for image processing, but helpful for building out the rest of the application.

    GStreamer (optional alternative)
        If you need a more industrial-grade pipeline for reading/writing/manipulating video streams in real-time.
        Generally used in streaming or complex media processing scenarios.

4. Example Pseudocode Putting It All Together

```cpp
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // 1. Open video
    cv::VideoCapture cap("gameplay.mp4");
    if(!cap.isOpened()) return -1;

    // 2. Initialize Tesseract (if OCR needed)
    tesseract::TessBaseAPI ocr;
    if (ocr.Init(nullptr, "eng")) { // Adjust language pack as needed
        // handle error
        return -1;
    }

    // 3. Main loop over frames
    cv::Mat frame;
    int frameCount = 0;
    while (true) {
        if(!cap.read(frame)) break;  // end of video

        frameCount++;
        
        // Optional: skip frames for performance
        if(frameCount % 5 != 0) continue;

        // 4. Detect Health Bar (example)
        cv::Rect healthBarROI(0, 0, 200, 25);
        cv::Mat healthBar = frame(healthBarROI);

        // Convert to HSV and threshold for green color
        cv::Mat hsv;
        cv::cvtColor(healthBar, hsv, cv::COLOR_BGR2HSV);
        cv::Mat mask;
        cv::inRange(hsv, cv::Scalar(50, 100, 100), cv::Scalar(70, 255, 255), mask);

        // Count non-zero pixels
        double filledPixels = cv::countNonZero(mask);
        double totalPixels = healthBarROI.width * healthBarROI.height;
        double healthPercent = filledPixels / totalPixels * 100.0;

        // 5. Detect Death Popup (example template matching)
        // For brevity, assume we preloaded a template, deathTemplate
        // ...
        // cv::Mat result;
        // cv::matchTemplate(frame, deathTemplate, result, cv::TM_CCOEFF_NORMED);
        // double minVal, maxVal; cv::Point minLoc, maxLoc;
        // cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        // bool isDead = (maxVal > 0.8); // threshold

        // 6. OCR for XP Text (pseudocode)
        /*
        cv::Rect xpRect(1000, 50, 80, 20);
        cv::Mat xpRoi = frame(xpRect);

        // Some preprocessing (convert to grayscale, threshold)
        cv::Mat grayXp;
        cv::cvtColor(xpRoi, grayXp, cv::COLOR_BGR2GRAY);
        cv::threshold(grayXp, grayXp, 150, 255, cv::THRESH_BINARY);

        ocr.SetImage(grayXp.data, grayXp.cols, grayXp.rows, 1, grayXp.step);
        std::string xpText = std::string(ocr.GetUTF8Text());

        // Parse xpText for integer values, etc.
        */

        // 7. Log or store the results
        // e.g., gameState.push_back({frameCount, healthPercent, isDead, xpValue, ...});
    }

    // 8. Cleanup
    ocr.End();
    cap.release();
    return 0;
}
```

5. Summary

    Frame-by-frame processing is usually sufficient; sample fewer frames if real-time performance is not needed.
    Fixed-position, color-coded bars are straightforward to detect with color thresholding and pixel counting.
    Popups and menu screens can be tackled with template matching or object detection if variability is high.
    Text-based UI elements require OCR (e.g., Tesseract) after appropriate preprocessing.
    OpenCV is the primary go-to for C++-based image processing. Combine it with Tesseract for OCR, and optionally an object detection framework (YOLO, etc.) for more robust/complex detections.

With this pipeline, you will be able to extract health, shield, and XP values, detect death screens or pop-ups, and interpret various states of the game’s UI. Adjust details (like threshold ranges, bounding box coordinates, model training) depending on the specifics of your game’s interface and visual design.


### Realtime Considerations

Assume that I want to do the same in realtime from a live screencapture. What modifications would you make to capture the screen and process the video in realtime?
Reasoned about real-time game state extraction for a couple of seconds

When transitioning from offline video analysis (i.e., reading from a file) to real-time screen capture, there are a few key modifications and additional considerations:
1. Capturing the Screen in Real Time
1.1 Platform-Specific APIs

    Windows
        GDI-based capture (BitBlt)
            Use BitBlt in combination with GetDC to grab the desktop image.
            This is relatively simple to implement but can be slow for high-framerate or high-resolution captures.
        Windows Desktop Duplication API (DXGI)
            A more modern, performant API for capturing the desktop at higher frame rates.
            Available on Windows 8 and later.
            Allows capturing not only the entire desktop but also sub-regions or specific windows.
    macOS
        Use the Quartz Display Services or CoreGraphics frameworks (e.g., CGDisplayCreateImage).
        If you need better performance, there are Metal-based approaches or third-party frameworks, but Quartz/CG is usually sufficient for basic real-time capture.
    Linux (X11)
        Use X11 functions like XGetImage to capture the screen.
        Alternatively, FFmpeg or GStreamer can capture X11.
        Under Wayland, you may need a compositor-specific protocol or use PipeWire (more advanced).
    Cross-Platform
        Use libraries like OpenCV (with some caveats—OpenCV doesn’t have a built-in “desktop capture” function, but you can read from a “desktop” input with FFmpeg or GStreamer if configured correctly).
        FFmpeg can capture the desktop on many platforms with input arguments like -f gdigrab (Windows) or -f x11grab (Linux X11).
        GStreamer has plugins (e.g., ximagesrc on Linux, dx9screencapsrc on Windows) for screen capture.

1.2 Example Approaches

    GDI + OpenCV (Windows-only, basic example)

```cpp
#include <opencv2/opencv.hpp>
#include <Windows.h>

cv::Mat captureScreen(const RECT& region) {
    // 1. Get the device context of the screen
    HDC hScreenDC = GetDC(nullptr); 
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    int width = region.right - region.left;
    int height = region.bottom - region.top;

    // 2. Create a compatible bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

    // 3. BitBlt from screen DC to memory DC
    BitBlt(hMemoryDC, 0, 0, width, height, 
           hScreenDC, region.left, region.top, SRCCOPY);

    // 4. Create OpenCV Mat from HBITMAP
    BITMAPINFO bmpInfo;
    ZeroMemory(&bmpInfo, sizeof(bmpInfo));
    bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height; // top-down
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    cv::Mat mat(height, width, CV_8UC3);
    GetDIBits(hMemoryDC, hBitmap, 0, height, mat.data, &bmpInfo, DIB_RGB_COLORS);

    // 5. Cleanup
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);

    return mat;
}
```

Windows Desktop Duplication API (far more efficient, recommended for high performance)

    Use IDXGIOutputDuplication to grab frames from the GPU.
    The code is more involved, but numerous examples online show how to map the captured GPU texture into a CPU-accessible buffer, then wrap that buffer into a cv::Mat.

FFmpeg Command-Line + OpenCV

    You can launch FFmpeg as a screen-grabbing source and pipe frames to OpenCV. For example, on Windows:

        ffmpeg -f gdigrab -framerate 30 -i desktop -vcodec rawvideo -pix_fmt bgr24 -f rawvideo -

        Then read from stdin in your C++ program with OpenCV. (Requires advanced piping setup.)

    GStreamer
        On Linux (X11): ximagesrc ! videoconvert ! appsink pipeline.
        On Windows: dx9screencapsrc ! videoconvert ! appsink.
        Then pull frames from the GStreamer appsink into your C++ code.

2. Processing the Frames in Real Time

Once you’ve captured frames from the screen, you can process them in almost exactly the same way you would process frames from a video file. The core difference is that now your “capture loop” is tied to the screen refresh rate or your chosen capture rate, rather than reading from a file.

    Main Loop
        In your real-time loop, you’ll capture one frame from the screen, then immediately pass it to the image processing pipeline.
        If you need to keep up with real-time, you should be sure your pipeline can handle frames at your desired capture frame rate (e.g., 30 FPS or 60 FPS).

    Performance Considerations
        Screen capture can be CPU/GPU intensive, especially at high resolutions (1080p, 1440p, 4K).
        Your image processing pipeline must also be efficient (avoid excessive copying, unnecessary conversions, etc.).
        If your algorithm is too slow, you can throttle capture or process fewer frames (e.g., process every other frame).

    Parallelization / Multithreading
        You might want to separate screen capture from processing by using a producer-consumer model:
            Producer thread: captures frames and places them in a queue.
            Consumer thread: takes frames from the queue, runs detection (health bar, pop-up detection, etc.), and logs results.
        This can help maintain a more stable frame rate for both capture and processing.

3. Updated Real-Time Pseudocode

Below is a rough sketch of how you might integrate real-time screen capture with the previously described UI detection pipeline using a GDI capture approach on Windows as an example. (Adjust for your platform/library of choice.)

```cpp
#include <opencv2/opencv.hpp>
#include <Windows.h>
// Additional includes (threading, queues, etc.)

// Same captureScreen function as shown above
cv::Mat captureScreen(const RECT& region);

int main() {
    // Define region to capture (fullscreen or sub-area)
    RECT screenRegion;
    screenRegion.left = 0;
    screenRegion.top = 0;
    screenRegion.right = 1920;  // or use GetSystemMetrics(SM_CXSCREEN)
    screenRegion.bottom = 1080; // or use GetSystemMetrics(SM_CYSCREEN)

    // Initialize any ML models, e.g., OpenCV DNN, Tesseract, etc.
    // ...

    // Frame rate control
    const double targetFps = 30.0;
    const double frameIntervalMs = 1000.0 / targetFps;

    // Real-time loop
    while (true) {
        // Time measurement for FPS control
        auto t1 = std::chrono::high_resolution_clock::now();

        // 1. Capture the screen
        cv::Mat frame = captureScreen(screenRegion);
        if (frame.empty()) {
            // handle error
            break;
        }

        // 2. [Optional] Downscale for performance
        // cv::resize(frame, frame, cv::Size(), 0.5, 0.5);

        // 3. Detect UI elements (same approach as with video)
        //    e.g. threshold health bar, check for popups, run OCR, etc.
        //    Example: Health bar detection
        cv::Rect healthBarROI(0, 0, 200, 25);
        cv::Mat healthBar = frame(healthBarROI);
        // color threshold, find fill percentage, etc.

        // 4. Display or store the result
        // If you want a debug window:
        cv::imshow("Live Capture", frame);
        if (cv::waitKey(1) == 27) { // ESC key to exit
            break;
        }

        // 5. Enforce target FPS
        auto t2 = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
        if (elapsedMs < frameIntervalMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(frameIntervalMs - elapsedMs)));
        }
    }

    return 0;
}
```

3.1 Threaded Approach (Producer-Consumer)

For more stable performance, you can decouple capture from analysis:

    Capture Thread:
        Captures frames using captureScreen.
        Pushes frames into a concurrent queue (e.g., std::queue<cv::Mat> protected by a mutex).
    Processing Thread:
        Pops frames from the queue, runs the detection pipeline, updates/records the game state.

This way, if detection is slow or occasionally spikes, it won’t block frame capture completely.
4. Key Points to Ensure Real-Time Performance

    Efficient Capture
        Windows Desktop Duplication API or GPU-based solutions typically outperform GDI for high-framerate captures.
    Avoid Excessive Copies
        Where possible, operate on the same buffers (e.g., skip extra Mat conversions).
        Move data around by reference instead of copying large images.
    Skip Frames / Downscale
        If your pipeline can’t keep up at full resolution, consider downscaling the captured frame or processing every nth frame.
    Hardware Acceleration
        Certain operations (e.g., color conversion, resizing) can be offloaded to GPU if supported by your library (OpenCV’s CUDA module, for instance).
    Profiling
        Use CPU/GPU profilers to find bottlenecks (screen capture, thresholding, OCR, etc.).

5. Summary

To adapt your offline video analysis pipeline to real-time screen capture:

    Replace Video I/O with screen capture logic—be it via GDI, Desktop Duplication, FFmpeg/GStreamer screen-grabbing, or platform-specific APIs.
    Maintain the same pipeline for detecting UI elements (bars, popups, etc.), but be mindful of performance constraints.
    Consider multithreading so that capture and processing run in parallel, improving responsiveness and stability.
    Scale the pipeline (downscale images, skip frames, optimize code) to maintain the desired real-time frame rate (e.g., 30 or 60 FPS).

These modifications should allow you to capture game state (health, shield, XP bars, death screens, menus) from a live screen in real time.

### Alternate Approach : CNN Regression

Once we have an annotated dataset, we could use a CNN/ViT with a regression head to predict the values and states in a more robust
and computationally efficient manner.

# Polygon ROI Extraction using PIL

You are a software engineer. Please write a function that takes, given a list of 2d coordinates representing the vertices fo a polygon in image space, retrieves the pixels bounded by the polygon. Please suggested suitable python libraries such as numpy or PIL/pillow.

Evaluating methods

I’m noting various Python libraries such as PIL, scikit-image, shapely, and rasterization, considering their effectiveness in retrieving pixel coordinates within a polygon.

Crafting a mask

I’m using PIL's ImageDraw to create an empty image mask, filling it with the polygon, and then pulling pixel data from the original image or the mask using NumPy.

Mapping the process

First, a mask is created for the polygon. Then, pixel coordinates and values are extracted using NumPy and PIL, focusing on pixels where the mask equals 1.

Below is one example solution in Python that uses Pillow (PIL) and NumPy. The main idea is:

    1. Create a blank (binary) mask of the same size as the image.
    2. Draw (fill) the polygon on that blank mask using the vertex coordinates.
    3. Convert the mask to a NumPy array and find which pixels are set to 1.
    4. Use these coordinates to retrieve the corresponding pixels from the original image (also as a NumPy array).

```python
from PIL import Image, ImageDraw
import numpy as np

def get_polygon_pixels(image_path, polygon):
    """
    Given a list of 2D coordinates (`polygon`) representing the vertices of 
    a polygon in the coordinate space of `image_path`, 
    this function returns:
      1) the (row, col) coordinates of all pixels inside the polygon
      2) the corresponding pixel values from the image.

    Args:
        image_path (str): Path to the image.
        polygon (list of tuples): A list of (x, y) vertex coordinates 
                                  defining the polygon in image space.
    Returns:
        coords (ndarray): An Nx2 array of (row, col) integer coordinates.
        polygon_pixels (ndarray): The pixel values at those coordinates.
    """
    
    # 1. Load the image with PIL
    image = Image.open(image_path).convert("RGB")
    width, height = image.size
    
    # 2. Create a blank (grayscale) mask and get a drawing context
    #    We'll fill the polygon region with 1 (white) on this mask.
    mask = Image.new('L', (width, height), 0)
    draw = ImageDraw.Draw(mask)
    
    # 3. Draw the polygon on the mask
    #    Note: PIL expects a sequence of (x, y) pairs.
    #    outline=1, fill=1 ensures the polygon interior is labeled as 1.
    draw.polygon(polygon, outline=1, fill=1)
    
    # 4. Convert the mask and the original image to NumPy arrays
    mask_np = np.array(mask)           # shape: (height, width)
    image_np = np.array(image)         # shape: (height, width, channels)
    
    # 5. Find where the mask is 1
    #    This returns a 2D array of coordinates [ [row1, col1], [row2, col2], ... ]
    coords = np.argwhere(mask_np == 1)
    
    # 6. Retrieve pixel values at these coordinates
    #    coords[:, 0] -> rows, coords[:, 1] -> cols
    polygon_pixels = image_np[coords[:, 0], coords[:, 1]]
    
    return coords, polygon_pixels

# Example usage:
# Suppose you have an image "example.jpg" and a polygon defined by some vertices:
# polygon_vertices = [(10, 10), (200, 50), (180, 180), (20, 150)]
# coords, pixels = get_polygon_pixels("example.jpg", polygon_vertices)
# 
# coords will be an array of shape (N, 2) with the (row, col) of each pixel.
# pixels will be an array of shape (N, 3) with the corresponding RGB values.
```

Explanation of the Approach

    1. Mask Creation
    We create a blank grayscale image (mask) with the same width/height as the original image, initialized to 0 (black). We then draw (fill) the polygon onto this mask in white (value = 1). The result is a binary mask where pixels inside the polygon are 1, and everything else is 0.

    2. Coordinate Extraction
    After converting the mask to a NumPy array, np.argwhere(mask_np == 1) finds all (row, col) indices where the mask is 1—i.e., inside the polygon.

    3. Pixel Retrieval
    Converting the original image into a NumPy array lets us index directly with these coordinates. We then collect all pixel values corresponding to those coordinates inside the polygon.

Alternative Approaches

    1. Using matplotlib.path
    You could use matplotlib.path.Path to create a path from the polygon vertices and call contains_points on a grid of coordinates. This is more direct for purely geometric operations but involves manual indexing of the image.

    2. Using shapely
    For more sophisticated geometry operations (e.g., union, intersection, buffering), Shapely is a powerful library. You could create a Polygon object and then iterate over pixel coordinates, checking if they are within the polygon. This is more flexible but can be slower for large images unless you apply some optimizations.

    3. Using scikit-image
    The scikit-image library has functions like skimage.draw.polygon, which also creates indices that you can fill into a mask or directly sample from an image array.

Recommended Libraries

    1. NumPy: For array operations and indexing.
    2. Pillow (PIL): Easy creation of masks and drawing polygons.
    3. (Optional) Shapely: Complex geometric operations (intersections, unions).
    4. (Optional) scikit-image: Image processing utilities, including polygon rasterization (skimage.draw.polygon).

Using Pillow and NumPy typically suffices for the basic task of extracting pixels within a polygon from an image.

# Real-time Inference

## Torch Models in C++

See https://www.reddit.com/r/pytorch/comments/xb5pxh/best_way_to_use_pytorch_model_in_a_c_application/
- If you trace and script your model in python, you can directly run it in C++ via torch::jit::load(). Another way is to export it to ONNX and then load it in C++ via ONNX's C API. 
    - this approach is documented in https://pytorch.org/tutorials/advanced/cpp_export.html
- If you don’t care about inference speed, then libtorch is the easiest way. If inference latency is the highest priority then do pytorch -> onnx -> tensorrt. Or using onnx runtime might also be a good middle ground. 


## Zero-copy Gstreamer to Tensor (Python)

* [A Simple and Flexible Pytorch Video Pipeline \| paulbridger.com](https://paulbridger.com/posts/video-analytics-pytorch-pipeline/ "A Simple and Flexible Pytorch Video Pipeline | paulbridger.com")
* [Getting the pointer to CUDAMemory of a buffer - General Discussion - GStreamer Discourse](https://discourse.gstreamer.org/t/getting-the-pointer-to-cudamemory-of-a-buffer/600 "Getting the pointer to CUDAMemory of a buffer - General Discussion - GStreamer Discourse")
* [Gstreamer NVMM memory buffer to torch tensor with zero copy - Intelligent Video Analytics / DeepStream SDK - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/gstreamer-nvmm-memory-buffer-to-torch-tensor-with-zero-copy/295501 "Gstreamer NVMM memory buffer to torch tensor with zero copy - Intelligent Video Analytics / DeepStream SDK - NVIDIA Developer Forums")
* [deepstream_python_apps/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py at master · NVIDIA\-AI\-IOT\/deepstream\_python\_apps](https://github.com/NVIDIA-AI-IOT/deepstream_python_apps/blob/master/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py#L121 "deepstream_python_apps/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py at master · NVIDIA-AI-IOT/deepstream_python_apps")



# Goal : Distributable with great UX

Make it into a binary distributable (both capture and inference) that has a good UX
so that it can be used by dummies.