#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

#include "BinaryIO.hpp"
#include "TestWindowsSetup.hpp"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestUint32RoundTrip() {
    std::stringstream stream;
    trajectory::WriteUint32LittleEndian(stream, 0x78563412u);
    Expect(stream.str() == std::string("\x12\x34\x56\x78", 4), "uint32 should be written in little-endian order");
    Expect(trajectory::ReadUint32LittleEndian(stream) == 0x78563412u, "uint32 should round-trip");
}

void TestLengthPrefixedRoundTrip() {
    std::stringstream stream;
    trajectory::WriteLengthPrefixedPayload(stream, "payload");
    Expect(trajectory::ReadLengthPrefixedPayload(stream) == "payload", "payload should round-trip");
}

void TestShortReadThrows() {
    const std::string truncated_payload{char(0x05), char(0x00), char(0x00), char(0x00), 'a', 'b', 'c'};
    std::stringstream stream(truncated_payload);
    bool threw = false;
    try {
        static_cast<void>(trajectory::ReadLengthPrefixedPayload(stream));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Expect(threw, "short reads should throw");
}

}  // namespace

int main() {
    trajectory::test_support::DisableWindowsErrorDialogs();
    TestUint32RoundTrip();
    TestLengthPrefixedRoundTrip();
    TestShortReadThrows();
    return 0;
}
