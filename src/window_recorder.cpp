#include "window_recorder.h"
#include <fstream>
#include <chrono>
#include <iostream>

WindowRecorder::WindowRecorder()
    : recording_(false)
    , target_window_(nullptr)
    , d3d_device_(nullptr)
    , d3d_context_(nullptr)
    , output_duplication_(nullptr)
    , output_(nullptr)
{
}

WindowRecorder::~WindowRecorder() {
    StopRecording();
    CleanupDXGI();
}

/**
 * @brief Starts recording the specified window and writes the output to a CSV file.
 * 
 * @param window_handle Handle to the window to be recorded.
 * @param output_file Path to the output CSV file where the recording data will be saved.
 * @return true if recording started successfully, false otherwise.
 * 
 * This function initializes the DXGI for capturing frames from the specified window.
 * It also opens the output CSV file and writes the header. If recording is already
 * in progress or if any initialization step fails, the function returns false.
 */
bool WindowRecorder::StartRecording(HWND window_handle, const std::string& output_file) {
    if (recording_) {
        return false;
    }

    target_window_ = window_handle;
    output_file_ = output_file;

    if (!InitializeDXGI()) {
        std::cerr << "Failed to initialize DXGI" << std::endl;
        return false;
    }

    frame_counter_ = 0;
    csv_file_.open(output_file_, std::ios::out);
    if (!csv_file_.is_open()) {
        return false;
    }

    // Write CSV header
    csv_file_ << "frame_number,timestamp\n";

    recording_ = true;
    return true;
}

void WindowRecorder::StopRecording() {
    if (!recording_) {
        return;
    }

    recording_ = false;
    csv_file_.close();
    CleanupDXGI();
}

/**
 * @brief Updates the window recording process.
 * 
 * This function captures the current frame of the target window if recording is active.
 * It retrieves the frame buffer, width, and height of the window. If a frame is successfully
 * captured, it logs the frame number and timestamp to a CSV file. The actual frame encoding
 * is currently omitted and will be implemented later.
 * 
 * @note This function does nothing if recording is not active.
 */
void WindowRecorder::Update() {
    if (!recording_) {
        return;
    }

    std::vector<uint8_t> frame_buffer;

    // TODO: use the dimensions/location of target_window_, or capture window directly instead of using desktop duplication
    uint32_t width = 0, height = 0;

    if (GetFrame(frame_buffer, width, height)) {
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        WCHAR title[256];
        GetWindowTextW(target_window_, title, 256);
        char narrow_title[256];
        WideCharToMultiByte(CP_UTF8, 0, title, -1, narrow_title, 256, NULL, NULL);

        // append frame number and timestamp to csv
        csv_file_ << frame_counter_ << "," << timestamp << "\n";
        frame_counter_++;

        // Placeholder: later we will encode the frame using ffmpeg, but for now we omit this
    }
}

bool WindowRecorder::InitializeDXGI() {
    // Create D3D11 device
    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &d3d_device_,
        &feature_level,
        &d3d_context_
    );

    if (FAILED(hr)) {
        return false;
    }

    // Get DXGI device
    IDXGIDevice* dxgi_device = nullptr;
    hr = d3d_device_->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
    if (FAILED(hr)) {
        return false;
    }

    // Get DXGI adapter
    IDXGIAdapter* dxgi_adapter = nullptr;
    hr = dxgi_device->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgi_adapter);
    dxgi_device->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Get output
    hr = dxgi_adapter->EnumOutputs(0, (IDXGIOutput**)&output_);
    dxgi_adapter->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Get output description
    hr = output_->GetDesc(&output_desc_);
    if (FAILED(hr)) {
        return false;
    }

    // Create desktop duplication
    hr = output_->DuplicateOutput(d3d_device_, &output_duplication_);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

/**
 * @brief Captures the next frame from the desktop duplication output.
 * 
 * @param buffer A reference to a vector that will be filled with the frame data.
 * @param width A reference to a uint32_t that will be set to the width of the captured frame.
 * @param height A reference to a uint32_t that will be set to the height of the captured frame.
 * @return true if the frame was successfully captured, false otherwise.
 * 
 * This function captures the next frame from the desktop duplication output and stores the frame data in the provided buffer.
 * It also sets the width and height of the captured frame. If the function fails to capture the frame, it returns false.
 * The function performs the following steps:
 * 1. Checks if the output duplication interface is valid.
 * 2. Attempts to acquire the next frame from the output duplication interface.
 * 3. Retrieves the texture interface from the acquired frame.
 * 4. Gets the description of the texture to determine the frame dimensions.
 * 5. Creates a staging texture for CPU access.
 * 6. Copies the desktop texture to the staging texture.
 * 7. Maps the staging texture to access the frame data.
 * 8. Copies the frame data to the provided buffer.
 * 9. Releases the resources and returns the result of the operation.
 */
bool WindowRecorder::GetFrame(std::vector<uint8_t>& buffer, uint32_t& width, uint32_t& height) {
    if (!output_duplication_) {
        return false;
    }

    IDXGIResource* desktop_resource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    
    // Try to get the next frame
    HRESULT hr = output_duplication_->AcquireNextFrame(
        100, // timeout in milliseconds
        &frame_info,
        &desktop_resource
    );

    if (FAILED(hr)) {
        return false;
    }

    // Get the texture interface
    ID3D11Texture2D* desktop_texture = nullptr;
    hr = desktop_resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktop_texture);
    desktop_resource->Release();

    if (FAILED(hr)) {
        return false;
    }

    // Get the texture description
    D3D11_TEXTURE2D_DESC texture_desc;
    desktop_texture->GetDesc(&texture_desc);
    width = texture_desc.Width;
    height = texture_desc.Height;

    // Create staging texture for CPU access
    D3D11_TEXTURE2D_DESC staging_desc = texture_desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.BindFlags = 0;
    staging_desc.MiscFlags = 0;

    ID3D11Texture2D* staging_texture = nullptr;
    hr = d3d_device_->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
    
    if (SUCCEEDED(hr)) {
        // Copy the desktop texture to staging texture
        d3d_context_->CopyResource(staging_texture, desktop_texture);

        // Map the staging texture
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        hr = d3d_context_->Map(staging_texture, 0, D3D11_MAP_READ, 0, &mapped_resource);
        
        if (SUCCEEDED(hr)) {
            // Calculate buffer size and resize
            size_t buffer_size = mapped_resource.RowPitch * height;
            buffer.resize(buffer_size);

            // Copy the data
            uint8_t* src = static_cast<uint8_t*>(mapped_resource.pData);
            for (uint32_t row = 0; row < height; row++) {
                memcpy(
                    buffer.data() + row * mapped_resource.RowPitch,
                    src + row * mapped_resource.RowPitch,
                    mapped_resource.RowPitch
                );
            }

            d3d_context_->Unmap(staging_texture, 0);
        }

        staging_texture->Release();
    }

    desktop_texture->Release();
    output_duplication_->ReleaseFrame();

    return SUCCEEDED(hr);
}

void WindowRecorder::CleanupDXGI() {
    if (output_duplication_) {
        output_duplication_->Release();
        output_duplication_ = nullptr;
    }
    if (output_) {
        output_->Release();
        output_ = nullptr;
    }
    if (d3d_context_) {
        d3d_context_->Release();
        d3d_context_ = nullptr;
    }
    if (d3d_device_) {
        d3d_device_->Release();
        d3d_device_ = nullptr;
    }
}
