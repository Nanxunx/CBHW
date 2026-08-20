#include "turtle335/m2/CameraWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
}

void PutU32(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
    d[o + 2u] = static_cast<std::uint8_t>((v >> 16u) & 0xffu);
    d[o + 3u] = static_cast<std::uint8_t>((v >> 24u) & 0xffu);
}

std::uint32_t GetU32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
           (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
           (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
}

void PutF32(std::vector<std::uint8_t>& d, const std::size_t o, const float v)
{
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(bits));
    PutU32(d, o, bits);
}

float GetF32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    const auto bits = GetU32(d, o);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    constexpr std::uint32_t cameraOff = 32u;
    std::vector<std::uint8_t> source(320u, 0u);

    // Fixed Type/FOV/far/near and static vectors.
    PutU32(source, cameraOff + 0u, 1u);
    PutF32(source, cameraOff + 4u, 0.75f);
    PutF32(source, cameraOff + 8u, 1000.0f);
    PutF32(source, cameraOff + 12u, 0.5f);
    PutF32(source, cameraOff + 36u, 1.0f);
    PutF32(source, cameraOff + 40u, 2.0f);
    PutF32(source, cameraOff + 44u, 3.0f);
    PutF32(source, cameraOff + 68u, 4.0f);
    PutF32(source, cameraOff + 72u, 5.0f);
    PutF32(source, cameraOff + 76u, 6.0f);

    // transPosition: one constant BigFloat36 key at source payload 220.
    PutU16(source, cameraOff + 16u + 0u, 1u);
    PutU16(source, cameraOff + 16u + 2u, 0xffffu);
    PutU32(source, cameraOff + 16u + 4u, 1u); PutU32(source, cameraOff + 16u + 8u, 180u);
    PutU32(source, cameraOff + 16u + 12u, 1u); PutU32(source, cameraOff + 16u + 16u, 188u);
    PutU32(source, 180u, 1u); PutU32(source, 184u, 204u);
    PutU32(source, 188u, 1u); PutU32(source, 192u, 220u);
    PutU32(source, 204u, 9u);
    for (std::size_t f = 0u; f < 9u; ++f)
        PutF32(source, 220u + f * 4u, static_cast<float>(f + 1u));

    // transTarget + roll empty.
    PutU16(source, cameraOff + 48u + 2u, 0xffffu);
    PutU16(source, cameraOff + 80u + 2u, 0xffffu);

    WotlkM2Sequence sequence{};
    sequence.animationId = 0u;
    sequence.length = 100u;
    sequence.flags = 0x20u;
    const std::vector<WotlkM2Sequence> sequences{sequence};
    const std::vector<ClassicSequenceWindow> windows{{3333u, 3433u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr};

    BinaryBuilder output(std::vector<std::uint8_t>(324u, 0u));
    const auto cameras = ConvertWotlkCameras(
        output,
        source,
        M2ArrayRef{1u, cameraOff},
        windows,
        sequences,
        sidecars);

    assert(cameras.count == 1u && cameras.offset == 324u);
    const auto& d = output.Bytes();
    const std::size_t c = cameras.offset;

    assert(GetU32(d, c + 0u) == 1u);
    assert(GetF32(d, c + 4u) == 0.75f);
    assert(GetF32(d, c + 8u) == 1000.0f);
    assert(GetF32(d, c + 12u) == 0.5f);
    assert(GetF32(d, c + 44u) == 1.0f);
    assert(GetF32(d, c + 48u) == 2.0f);
    assert(GetF32(d, c + 52u) == 3.0f);
    assert(GetF32(d, c + 84u) == 4.0f);
    assert(GetF32(d, c + 88u) == 5.0f);
    assert(GetF32(d, c + 92u) == 6.0f);

    // Constant BigFloat camera translation stays one 36-byte key/no ranges.
    assert(GetU32(d, c + 16u + 4u) == 0u);
    assert(GetU32(d, c + 16u + 12u) == 1u);
    assert(GetU32(d, c + 16u + 20u) == 1u);
    const auto keyOff = GetU32(d, c + 16u + 24u);
    for (std::size_t f = 0u; f < 9u; ++f)
        assert(GetF32(d, keyOff + f * 4u) == static_cast<float>(f + 1u));

    std::cout << "PASS camera writer\n";
    return 0;
}
