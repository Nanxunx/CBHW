#include "turtle335/m2/LightWriter.h"

#include <cassert>
#include <cstdint>
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

} // namespace

int main()
{
    using namespace turtle335::m2;

    constexpr std::uint32_t lightOff = 32u;
    std::vector<std::uint8_t> source(lightOff + 156u, 0u);
    PutU16(source, lightOff + 0u, 2u);
    PutU16(source, lightOff + 2u, 7u);
    for (std::size_t i = 0u; i < 12u; ++i)
        source[lightOff + 4u + i] = static_cast<std::uint8_t>(0x30u + i);

    // Seven empty WotLK tracks; global sequence = -1.
    for (const std::size_t off : {16u,36u,56u,76u,96u,116u,136u})
        PutU16(source, lightOff + off + 2u, 0xffffu);

    WotlkM2Sequence sequence{};
    sequence.animationId = 0u;
    sequence.length = 100u;
    sequence.flags = 0x20u;
    const std::vector<WotlkM2Sequence> sequences{sequence};
    const std::vector<ClassicSequenceWindow> windows{{3333u,3433u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr};

    BinaryBuilder output(std::vector<std::uint8_t>(324u,0u));
    const auto lights = ConvertWotlkLights(
        output,
        source,
        M2ArrayRef{1u,lightOff},
        windows,
        sequences,
        sidecars);
    assert(lights.count == 1u && lights.offset == 324u);
    const auto& d = output.Bytes();
    const std::size_t l = lights.offset;
    assert(d[l+0u] == 2u && d[l+1u] == 0u);
    assert(d[l+2u] == 7u && d[l+3u] == 0u);
    for (std::size_t i = 0u; i < 12u; ++i)
        assert(d[l+4u+i] == static_cast<std::uint8_t>(0x30u+i));

    // All empty target animation blocks retain zero arrays.
    for (const std::size_t off : {16u,44u,72u,100u,128u,156u,184u})
    {
        assert(GetU32(d,l+off+4u) == 0u);
        assert(GetU32(d,l+off+12u) == 0u);
        assert(GetU32(d,l+off+20u) == 0u);
    }

    std::cout << "PASS light writer reference layout\n";
    return 0;
}
