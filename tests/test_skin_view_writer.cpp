#include "turtle335/m2/SkinViewWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void PutU32(std::vector<std::uint8_t>& d, std::size_t o, std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
    d[o + 2] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
    d[o + 3] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
}

std::uint32_t U32(const std::vector<std::uint8_t>& d, std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1]) << 8) |
           (static_cast<std::uint32_t>(d[o + 2]) << 16) |
           (static_cast<std::uint32_t>(d[o + 3]) << 24);
}

std::vector<std::uint8_t> MakeSkin()
{
    constexpr std::size_t indices = 48u;
    constexpr std::size_t triangles = 52u;
    constexpr std::size_t properties = 56u;
    constexpr std::size_t submeshes = 60u;
    constexpr std::size_t textureUnits = 108u;
    std::vector<std::uint8_t> d(132u, 0u);
    std::memcpy(d.data(), "SKIN", 4u);
    PutU32(d, 4u, 2u); PutU32(d, 8u, indices);
    PutU32(d, 12u, 2u); PutU32(d, 16u, triangles);
    PutU32(d, 20u, 1u); PutU32(d, 24u, properties);
    PutU32(d, 28u, 1u); PutU32(d, 32u, submeshes);
    PutU32(d, 36u, 1u); PutU32(d, 40u, textureUnits);
    PutU32(d, 44u, 7u);

    d[indices] = 1u; d[indices + 2u] = 2u;
    d[triangles] = 3u; d[triangles + 2u] = 4u;
    d[properties] = 9u; d[properties + 1u] = 8u;
    d[properties + 2u] = 7u; d[properties + 3u] = 6u;
    for (std::size_t i = 0; i < 48u; ++i)
        d[submeshes + i] = static_cast<std::uint8_t>(i + 1u);
    for (std::size_t i = 0; i < 24u; ++i)
        d[textureUnits + i] = static_cast<std::uint8_t>(100u + i);
    return d;
}

} // namespace

int main()
{
    using namespace turtle335::m2;
    const auto skin = MakeSkin();
    constexpr std::uint32_t base = 1000u;
    const auto out = BuildClassicEmbeddedViews({skin}, base);

    assert(out.size() >= kClassicViewHeaderSize);
    assert(U32(out, 0u) == 2u);
    assert(U32(out, 8u) == 2u);
    assert(U32(out, 16u) == 1u);
    assert(U32(out, 24u) == 1u);
    assert(U32(out, 32u) == 1u);
    assert(U32(out, 40u) == 7u);

    const auto submeshAbsolute = U32(out, 28u);
    const std::size_t submesh = static_cast<std::size_t>(submeshAbsolute - base);
    for (std::size_t i = 0; i < 32u; ++i)
        assert(out[submesh + i] == static_cast<std::uint8_t>(i + 1u));

    const auto textureUnitAbsolute = U32(out, 36u);
    const std::size_t textureUnit = static_cast<std::size_t>(textureUnitAbsolute - base);
    for (std::size_t i = 0; i < 24u; ++i)
        assert(out[textureUnit + i] == static_cast<std::uint8_t>(100u + i));

    std::cout << "PASS skin view writer\n";
    return 0;
}
