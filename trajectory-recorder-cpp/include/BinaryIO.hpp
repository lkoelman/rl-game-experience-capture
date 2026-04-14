#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace trajectory {

// Shared framing helper for on-disk binary records written by InputLogger.
inline void WriteUint32LittleEndian(std::ostream& out, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8u) & 0xffu),
        static_cast<char>((value >> 16u) & 0xffu),
        static_cast<char>((value >> 24u) & 0xffu),
    };
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("failed to write uint32_t value");
    }
}

// Reads the fixed-width record prefix used by actions.bin.
inline std::uint32_t ReadUint32LittleEndian(std::istream& in) {
    const auto b0 = in.get();
    const auto b1 = in.get();
    const auto b2 = in.get();
    const auto b3 = in.get();
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) {
        throw std::runtime_error("unexpected end of stream while reading uint32_t");
    }

    return static_cast<std::uint32_t>(static_cast<unsigned char>(b0)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b1)) << 8u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b2)) << 16u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b3)) << 24u);
}

// Writes one binary record as [u32 length][payload].
inline void WriteLengthPrefixedPayload(std::ostream& out, const std::string& payload) {
    if (payload.size() > UINT32_MAX) {
        throw std::runtime_error("payload exceeds 32-bit length prefix");
    }
    WriteUint32LittleEndian(out, static_cast<std::uint32_t>(payload.size()));
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!out) {
        throw std::runtime_error("failed to write payload");
    }
}

// Reads one binary record from the framing used by actions.bin.
inline std::string ReadLengthPrefixedPayload(std::istream& in) {
    const auto length = ReadUint32LittleEndian(in);
    std::string payload(length, '\0');
    if (!payload.empty()) {
        in.read(&payload[0], static_cast<std::streamsize>(payload.size()));
    }
    if (in.gcount() != static_cast<std::streamsize>(payload.size())) {
        throw std::runtime_error("unexpected end of stream while reading payload");
    }
    return payload;
}

}  // namespace trajectory
