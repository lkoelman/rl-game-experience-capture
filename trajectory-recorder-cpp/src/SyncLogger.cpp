#include "SyncLogger.hpp"

#include <stdexcept>

namespace trajectory {

SyncLogger::SyncLogger(const std::string& filepath) : out_csv_(filepath, std::ios::out | std::ios::trunc) {
    if (!out_csv_.is_open()) {
        throw std::runtime_error("failed to open sync csv: " + filepath);
    }
    out_csv_ << "frame_index,monotonic_ns,pts\n";
}

SyncLogger::~SyncLogger() = default;

void SyncLogger::LogFrame(std::uint64_t monotonic_ns, std::uint64_t pts) {
    // The pad probe may be invoked off the caller thread, so CSV appends are serialized here.
    std::lock_guard<std::mutex> lock(mtx_);
    out_csv_ << frame_count_++ << ',' << monotonic_ns << ',' << pts << '\n';
    if (!out_csv_) {
        throw std::runtime_error("failed to append sync row");
    }
}

}  // namespace trajectory
