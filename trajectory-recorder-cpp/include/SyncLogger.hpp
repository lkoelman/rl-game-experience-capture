#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace trajectory {

class SyncLogger {
public:
    explicit SyncLogger(const std::string& filepath);
    ~SyncLogger();

    void LogFrame(std::uint64_t monotonic_ns, std::uint64_t pts);

private:
    std::ofstream out_csv_;
    std::mutex mtx_;
    std::uint64_t frame_count_{0};
};

}  // namespace trajectory

