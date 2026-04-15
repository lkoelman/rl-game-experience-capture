# Gamepad Input

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


# Video Handling

- Decord notes
    - decord with PyTorch bridge: https://github.com/thu-ml/controlvideo/blob/main/dataset.py


# Real-time Inference

## Torch Models in C++

See https://www.reddit.com/r/pytorch/comments/xb5pxh/best_way_to_use_pytorch_model_in_a_c_application/
- If you trace and script your model in python, you can directly run it in C++ via torch::jit::load(). Another way is to export it to ONNX and then load it in C++ via ONNX's C API.
    - this approach is documented in https://pytorch.org/tutorials/advanced/cpp_export.html
- If you don’t care about inference speed, then libtorch is the easiest way. If inference latency is the highest priority then do pytorch -> onnx -> tensorrt. Or using onnx runtime might also be a good middle ground.


## Zero-copy Gstreamer to Tensor (Python)

* [A Simple and Flexible Pytorch Video Pipeline \| paulbridger.com](https://paulbridger.com/posts/video-analytics-pytorch-pipeline/ "A Simple and Flexible Pytorch Video Pipeline | paulbridger.com")
* [Getting the pointer to CUDAMemory of a buffer](https://discourse.gstreamer.org/t/getting-the-pointer-to-cudamemory-of-a-buffer/600)
* [Gstreamer NVMM memory buffer to torch tensor with zero copy](https://forums.developer.nvidia.com/t/gstreamer-nvmm-memory-buffer-to-torch-tensor-with-zero-copy/295501)
- `cudaImportExternalMemory` example: https://forums.developer.nvidia.com/t/cuda-interop-with-d3d12-possible/54671/4
    - could expose this function to python using ctypes: https://vitalitylearning.medium.com/using-c-c-and-cuda-functions-as-regular-python-functions-716f01f7ca22
* [deepstream_python_apps/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py at master · NVIDIA\-AI\-IOT\/deepstream\_python\_apps](https://github.com/NVIDIA-AI-IOT/deepstream_python_apps/blob/master/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py#L121 "deepstream_python_apps/apps/deepstream-imagedata-multistream/deepstream_imagedata-multistream.py at master · NVIDIA-AI-IOT/deepstream_python_apps")


## NVMM + NITROS Mock Interview

### Design Deep Dive – The Perception Stack

**Interviewer:** "Let’s pivot to the Perception stack. You have an NVIDIA Jetson Orin NX, and you’re reserving roughly half of its compute for the SLAM/VIO stack we just discussed. Walk me through the exact models and algorithms you'd use for detecting and tracking threats, and critically, how you would implement the software architecture and data flow to ensure we don't drop frames or choke the CPU."

**Candidate:** "This is where we need to be extremely disciplined about compute and memory management. To achieve 30 FPS on the remaining Orin NX resources, we must optimize both the mathematical models and the low-level memory handling.

Here is my breakdown of the models, the optimization strategy, and the software pipeline:

#### 1. Models and Algorithms
* **Object Detection: YOLO (Nano or Small variant)**
    * **Choice:** I would use a lightweight, modern single-stage detector like YOLOv10-N or YOLO11-N.
    * **Why:** Single-stage detectors are inherently faster than two-stage detectors (like Faster R-CNN). The 'Nano' variants are highly parameterized for edge devices. They offer the best trade-off between mean Average Precision (mAP) and inference latency.
* **Multi-Object Tracking (MOT): ByteTrack**
    * **Choice:** For assigning IDs to threats and tracking them across frames, I would use **ByteTrack**, rather than older algorithms like Deep SORT.
    * **Why:** This is a crucial compute-saving decision. Deep SORT relies on a secondary "Re-Identification" (Re-ID) neural network to extract visual features of a target, which requires a second pass through the GPU. ByteTrack, however, associates objects purely using a Kalman Filter (predicting where the box will be) and the Hungarian Algorithm (matching predicted boxes to new detections via Intersection-over-Union, or IoU). Furthermore, ByteTrack cleverly uses *low-confidence* bounding boxes to maintain tracks when targets are partially obscured by smoke or ruins, making it highly robust for combat zones without needing extra neural network overhead.



#### 2. Model Optimization for the Edge
We cannot run PyTorch natively on the drone for inference; it's too heavy.
* **TensorRT & Quantization:** I will export the trained YOLO model to ONNX, and then compile it using **NVIDIA TensorRT**. During compilation, I will apply **INT8 Quantization**.
* **Why:** Converting the model weights from 32-bit floats (FP32) to 8-bit integers (INT8) dramatically reduces memory bandwidth and takes advantage of the specialized Tensor Cores on the Orin NX. We will use a representative dataset of urban combat imagery during quantization to calibrate the network so we don't lose significant accuracy.

#### 3. Software Implementation and Data Flow
The biggest bottleneck in edge vision systems is often not the neural network itself, but memory copying. If we copy a 1080p image from the GPU to the CPU and back to the GPU for different nodes, latency will skyrocket.

To solve this, I will use **ROS 2** paired with **NVIDIA Isaac ROS (NITROS)** to implement a **Zero-Copy Data Flow**.

* **The Pipeline:**
    1. **Camera Node (Hardware Accelerated):** Uses `libargus` to pull frames directly from the MIPI CSI camera interface into GPU memory (NVMM). The CPU never touches the image matrix.
    2. **Pre-processing Node:** Resizes and normalizes the image tensor directly in GPU memory.
    3. **TensorRT Detection Node:** Runs the INT8 YOLO inference. It outputs a lightweight array of bounding boxes `[x, y, width, height, class, confidence]`.
    4. **ByteTrack Node:** Takes the array of bounding boxes (not the image) and runs the Kalman filter updates to assign Threat IDs.
    5. **Aggregator Node:** Converts the final tracked objects into a compact JSON or Protobuf message and sends it to the Blackboard (for the Behavior Tree) and the Comm Node (for ground transmission).



**Candidate:** "By strictly keeping the heavy image data in GPU memory (Zero-Copy) and stripping out the Re-ID network from our tracking algorithm, we ensure the perception stack runs at >30 FPS with minimal CPU utilization, leaving plenty of headroom for our SLAM and navigation tasks.

Does this address your concerns regarding the models and the software implementation, or would you like to dig into how we handle false positives or specific edge cases in the tracking?"

***

**Interviewer (Simulated):** "The choice of ByteTrack to avoid the Re-ID network penalty is very smart given our SWaP constraints, and utilizing zero-copy transport is exactly what I was looking for on the software implementation side."

As the candidate, how would you like to proceed? We have time to address the edge cases (like false positives/occlusion) you just mentioned, or we can move into **Step 4: Wrap-up** to discuss system bottlenecks, failure states, and scaling.

### Zero-copy Data Flow

**Prompt:** dive into the zero-copy data flow: how is the data passed between different components of the pipeline? How are the components implemented (as ros nodes? in python? ..)

**Interviewer (Simulated):** "Let's dive into that zero-copy data flow. Memory bandwidth is often the silent killer in edge AI, especially on a shared-memory architecture like the Jetson. Exactly how is the data passed between these different components without copying? And from a software engineering perspective, how are these components actually implemented—are you writing standard Python ROS nodes, or something else?"

**Candidate:** "That is the exact right question to ask, because if we build this using standard Python ROS 2 nodes, our system will fail to meet the latency requirements, even with a tiny model.

Here is the deep dive into the software implementation and the exact mechanics of the zero-copy pipeline.

#### 1. Implementation: Language and Framework
* **C++ is Mandatory:** The entire perception pipeline must be written in **C++** using `rclcpp`. Python (`rclpy`) introduces the Global Interpreter Lock (GIL) and inevitably triggers memory serialization/deserialization overhead when converting C++ backend objects to Python objects.
* **ROS 2 Intra-Process Communication (Components):** Standard ROS 2 nodes run in separate processes and use a DDS (Data Distribution Service) middleware to pass messages. Even on the same machine, this serializes the data over the local network stack. Instead, I will implement the nodes as **ROS 2 Components** (`rclcpp_components`). This allows us to load the Camera Node, Pre-processing Node, and TensorRT Node into a **single, shared executable process**.

#### 2. The Mechanics of Zero-Copy (Type Adaptation & NVMM)
The Jetson Orin NX is a Unified Memory Architecture (UMA)—the CPU and GPU share the same physical RAM chips. However, by default, they use separate virtual memory spaces, so data is still copied back and forth.

To achieve true zero-copy, we use **NVIDIA Isaac ROS (NITROS)**, which leverages a concept called Hardware Accelerated Type Adaptation:
1. **NVMM (NVIDIA Memory Management):** The camera driver (`libargus`) allocates a contiguous block of memory in the hardware-accessible NVMM space.
2. **Passing Pointers, Not Arrays:** When the Camera Node 'publishes' an image, it does not publish a standard `sensor_msgs/Image` (which contains a massive array of pixels). Instead, it publishes a custom message containing a **File Descriptor (FD)** or an **EGLStream pointer** that points to that specific block of NVMM.
3. **`std::unique_ptr`:** Because the nodes are in the same process, ROS 2 intra-process communication passes this pointer using `std::unique_ptr`. This guarantees that ownership is transferred between nodes in micro-seconds without a single byte of the image matrix being duplicated.

#### 3. The Data Flow Pipeline
Here is exactly how the data moves frame-by-frame:

1. **Hardware Ingestion (Camera Node):** The MIPI CSI camera writes pixels directly into an NVMM buffer. The Camera component passes a `unique_ptr` of the buffer ID to the next node. The CPU does 0 work.
2. **Image Scaling (Pre-processing Node):** This node receives the pointer. Instead of using OpenCV on the CPU (which would drag the data to CPU cache), it sends the pointer to the **VIC (Video Image Compositor)**, a dedicated hardware engine on the Jetson. The VIC resizes the image to 640x640, writes it to a *new* NVMM buffer, and passes the new pointer downstream. The CPU still does 0 work.
3. **Inference (TensorRT Node):** The GPU's Tensor Cores read directly from that new NVMM buffer to execute the YOLO INT8 model.
4. **The Transition (ByteTrack Node):** Up until now, the image never touched the CPU. The output of the TensorRT node is just an array of bounding boxes `[x, y, w, h, class, conf]`. Because this data is tiny (a few kilobytes), we can now safely copy it from GPU memory to standard CPU memory. The ByteTrack node (running on the CPU) takes these boxes and assigns the tracking IDs.

***


**Candidate:** "By building this entirely as intra-process C++ components and passing NVMM file descriptors, we bypass the CPU cache entirely for the heavy lifting. This is how we guarantee the perception stack won't interfere with the flight controller or the EGO-Planner running on the CPU. Does this clarify the software architecture and the data flow between components?"


**Interviewer (Simulated):** "Excellent answer. Moving from standard DDS messages to intra-process pointer passing, and utilizing the VIC for resizing, demonstrates a deep understanding of the edge hardware."

Where would you like to take the interview next? We can discuss handling the unreliable network connection (Comm Node), or we can move to **Step 4: Wrap-up** to summarize the system and discuss bottlenecks, scaling, or edge cases.


### NVMM Zero-Copy C++ Example

**Prompt:** write some mock code that shows how to pass image data via NVMM

**Interviewer (Simulated):** "I love the theory behind the zero-copy architecture. To prove you know how to build it, can you write some mock C++ code demonstrating exactly how this NVMM pointer passing looks in a ROS 2 component? I want to see how you avoid the CPU copy."

**Candidate:** "Absolutely. To do this, we rely on ROS 2's intra-process communication. When nodes are running in the same process, ROS 2 looks for messages published as a `std::unique_ptr`. If it sees one, it just passes the memory address to the subscriber instead of serializing the data.

In the NVIDIA Jetson ecosystem (like Isaac ROS), we don't pass standard `sensor_msgs::msg::Image`. Instead, we pass a message containing a **hardware buffer file descriptor (FD)** or an `NvBufSurface` pointer.

Here is a simplified mock-up of how the Producer (Camera) and Consumer (TensorRT) components interact in C++:"

```cpp
#include <rclcpp/rclcpp.hpp>
#include <memory>
#include <utility>

// ---------------------------------------------------------
// 1. Mock Custom Message for Hardware Buffers
// In a real system, we use NVIDIA Isaac ROS (NITROS) types,
// but conceptually it looks like this:
// ---------------------------------------------------------
struct NvmmBufferMsg {
    int hardware_fd;      // File Descriptor pointing to the NVMM block
    uint32_t width;
    uint32_t height;

    // Type alias for ROS 2 intra-process zero-copy
    using UniquePtr = std::unique_ptr<NvmmBufferMsg>;
};

// ---------------------------------------------------------
// 2. The Producer: Camera Node
// ---------------------------------------------------------
class CameraComponent : public rclcpp::Node {
public:
    CameraComponent(const rclcpp::NodeOptions & options)
    : Node("camera_node", options) {
        // Initialize publisher
        publisher_ = this->create_publisher<NvmmBufferMsg>("hw_image_stream", 10);

        // Simulate a hardware interrupt firing at 30 FPS
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(33),
            std::bind(&CameraComponent::capture_and_publish, this));
    }

private:
    void capture_and_publish() {
        // 1. libargus captures image DIRECTLY into GPU/NVMM.
        // We get a file descriptor back. The CPU does not touch the pixels.
        int nvmm_fd = mock_hardware_capture();

        // 2. Allocate our lightweight message container
        auto msg = std::make_unique<NvmmBufferMsg>();
        msg->hardware_fd = nvmm_fd;
        msg->width = 1920;
        msg->height = 1080;

        // 3. Publish via std::move.
        // Because it's a unique_ptr and we are using intra-process comms,
        // ROS 2 just hands the pointer to the subscriber. Zero copies made.
        RCLCPP_DEBUG(this->get_logger(), "Publishing NVMM FD: %d", msg->hardware_fd);
        publisher_->publish(std::move(msg));
    }

    int mock_hardware_capture() { return 42; } // Mock file descriptor

    rclcpp::Publisher<NvmmBufferMsg>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

// ---------------------------------------------------------
// 3. The Consumer: TensorRT Inference Node
// ---------------------------------------------------------
class TensorRTComponent : public rclcpp::Node {
public:
    TensorRTComponent(const rclcpp::NodeOptions & options)
    : Node("tensorrt_node", options) {
        // Initialize subscriber to expect a UniquePtr
        subscription_ = this->create_subscription<NvmmBufferMsg>(
            "hw_image_stream", 10,
            std::bind(&TensorRTComponent::inference_callback, this, std::placeholders::_1));
    }

private:
    // Notice the parameter is a UniquePtr. We now own the NVMM buffer pointer.
    void inference_callback(NvmmBufferMsg::UniquePtr msg) {
        RCLCPP_DEBUG(this->get_logger(), "Received NVMM FD: %d", msg->hardware_fd);

        // 1. Map the hardware buffer directly into the GPU's CUDA context
        void* cuda_device_ptr = mock_map_fd_to_cuda(msg->hardware_fd);

        // 2. Execute TensorRT inference. The GPU reads directly from the NVMM block.
        // The CPU still has not touched a single pixel of the image.
        mock_run_tensorrt_engine(cuda_device_ptr);

        // 3. When 'msg' goes out of scope here, the unique_ptr is destroyed,
        // and we can release the hardware buffer back to the camera pool.
    }

    void* mock_map_fd_to_cuda(int fd) { return nullptr; }
    void mock_run_tensorrt_engine(void* ptr) {}

    rclcpp::Subscription<NvmmBufferMsg>::SharedPtr subscription_;
};
```

**Candidate:** "As you can see, the 'magic' happens with `std::move(msg)` and the `UniquePtr` signature in the callback. The heavy 1080p image matrix stays locked in the NVMM partition, and the CPU only ever handles an integer file descriptor. To make this actually run, both of these classes would be loaded into a single component container executable at runtime."


**Interviewer (Simulated):** "That perfectly illustrates the concept. It shows you understand modern ROS 2 memory management down to the component level."
