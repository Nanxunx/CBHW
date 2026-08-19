#include "turtle335/m2/BoneWriter.h"

#include <cassert>
#include <cmath>
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

    constexpr std::size_t bone = 32u;
    std::vector<std::uint8_t> source(244u, 0u);

    // Fixed source bone fields.
    PutU32(source, bone + 0u, 7u);
    PutU32(source, bone + 4u, 0x1234u);
    PutU16(source, bone + 8u, 0xffffu);
    PutU16(source, bone + 10u, 3u);
    PutU32(source, bone + 12u, 0xdeadbeefu); // WotLK-only field: dropped.
    for (std::size_t i = 0u; i < 12u; ++i)
        source[bone + 76u + i] = static_cast<std::uint8_t>(0x80u + i);

    // Empty translation track.
    PutU16(source, bone + 16u + 2u, 0xffffu);

    // One constant compressed quaternion rotation key.
    PutU16(source, bone + 36u + 0u, 1u);
    PutU16(source, bone + 36u + 2u, 0xffffu);
    PutU32(source, bone + 36u + 4u, 1u); PutU32(source, bone + 36u + 8u, 200u);
    PutU32(source, bone + 36u + 12u, 1u); PutU32(source, bone + 36u + 16u, 208u);
    PutU32(source, 200u, 1u); PutU32(source, 204u, 216u);
    PutU32(source, 208u, 1u); PutU32(source, 212u, 220u);
    PutU32(source, 216u, 123u);
    PutU16(source, 220u, static_cast<std::uint16_t>(-32767));
    PutU16(source, 222u, static_cast<std::uint16_t>(-32767));
    PutU16(source, 224u, static_cast<std::uint16_t>(-32767));
    PutU16(source, 226u, static_cast<std::uint16_t>(-1));

    // One empty per-sequence scaling group. Target must synthesize identity
    // start/end values rather than zero scale.
    PutU16(source, bone + 56u + 0u, 1u);
    PutU16(source, bone + 56u + 2u, 0xffffu);
    PutU32(source, bone + 56u + 4u, 1u); PutU32(source, bone + 56u + 8u, 228u);
    PutU32(source, bone + 56u + 12u, 1u); PutU32(source, bone + 56u + 16u, 236u);
    PutU32(source, 228u, 0u); PutU32(source, 232u, 0u);
    PutU32(source, 236u, 0u); PutU32(source, 240u, 0u);

    WotlkM2Sequence sequence{};
    sequence.animationId = 0u;
    sequence.length = 100u;
    sequence.flags = 0x20u;
    const std::vector<WotlkM2Sequence> sequences{sequence};
    const std::vector<ClassicSequenceWindow> windows{{3333u, 3433u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr};

    BinaryBuilder output(std::vector<std::uint8_t>(324u, 0u));
    const auto result = ConvertWotlkBones(
        output,
        source,
        M2ArrayRef{1u, static_cast<std::uint32_t>(bone)},
        windows,
        sequences,
        sidecars);

    assert(result.target.count == 1u);
    assert(result.target.offset == 324u);
    const auto& d = output.Bytes();
    const std::size_t target = result.target.offset;

    assert(GetU32(d, target + 0u) == 7u);
    assert(GetU32(d, target + 4u) == 0x1234u);
    for (std::size_t i = 0u; i < 12u; ++i)
        assert(d[target + 96u + i] == static_cast<std::uint8_t>(0x80u + i));

    // Constant rotation: no ranges, one raw time/key. Key is float4 identity.
    assert(GetU32(d, target + 40u + 4u) == 0u);
    assert(GetU32(d, target + 40u + 12u) == 1u);
    assert(GetU32(d, target + 40u + 20u) == 1u);
    const std::uint32_t rotKeyOffset = GetU32(d, target + 40u + 24u);
    assert(GetF32(d, rotKeyOffset + 0u) == 0.0f);
    assert(GetF32(d, rotKeyOffset + 4u) == 0.0f);
    assert(GetF32(d, rotKeyOffset + 8u) == 0.0f);
    assert(GetF32(d, rotKeyOffset + 12u) == 1.0f);

    // Empty scale group: two range-backed keys and both are identity scale.
    assert(GetU32(d, target + 68u + 4u) == 2u);
    assert(GetU32(d, target + 68u + 12u) == 2u);
    assert(GetU32(d, target + 68u + 20u) == 2u);
    const std::uint32_t scaleKeyOffset = GetU32(d, target + 68u + 24u);
    for (std::size_t key = 0u; key < 2u; ++key)
    {
        assert(GetF32(d, scaleKeyOffset + key * 12u + 0u) == 1.0f);
        assert(GetF32(d, scaleKeyOffset + key * 12u + 4u) == 1.0f);
        assert(GetF32(d, scaleKeyOffset + key * 12u + 8u) == 1.0f);
    }

    std::cout << "PASS bone writer\n";
    return 0;
}
