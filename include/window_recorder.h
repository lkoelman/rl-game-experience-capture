#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>
#include <memory>
#include <vector>
#include "window_capture.pb.h"

class WindowRecorder {
public:
    WindowRecorder();
    ~WindowRecorder();

    bool StartRecording(HWND window_handle, const std::string& output_file);
    void StopRecording();
    void Update();

private:
    bool InitializeDXGI();
    bool GetFrame(std::vector<uint8_t>& buffer, uint32_t& width, uint32_t& height);
    void CleanupDXGI();

    bool recording_;
    HWND target_window_;
    recorder::WindowRecording recording_data_;
    std::string output_file_;

    // DXGI resources
    ID3D11Device* d3d_device_;
    ID3D11DeviceContext* d3d_context_;
    IDXGIOutputDuplication* output_duplication_;
    IDXGIOutput1* output_;
    DXGI_OUTPUT_DESC output_desc_;
};
