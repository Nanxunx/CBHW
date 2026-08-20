#include "turtle335/m2/ClassicM2Header.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

std::uint32_t U32(
    const std::array<std::uint8_t, turtle335::m2::kClassicM2HeaderSize>& d,
    std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1]) << 8) |
           (static_cast<std::uint32_t>(d[o + 2]) << 16) |
           (static_cast<std::uint32_t>(d[o + 3]) << 24);
}

} // namespace

int main()
{
    using namespace turtle335::m2;
    ClassicM2HeaderFields f{};
    f.nameLength = 12u;
    f.nameOffset = 324u;
    f.globalFlags = CanonicalizeClassicGlobalFlags(0x9u);
    f.animations = {3u, 400u};
    f.playableAnimationLookup = {226u, 1000u};
    f.views = {2u, 2000u};
    f.particles = {4u, 3000u};
    for (std::size_t i = 0; i < f.boundsAndCollisionFloats.size(); ++i)
        f.boundsAndCollisionFloats[i] = static_cast<std::uint8_t>(i);

    const auto h = BuildClassicM2Header(f);
    assert(std::memcmp(h.data(), "MD20", 4u) == 0);
    assert(U32(h, 4u) == 256u);
    assert(U32(h, 0x10u) == 1u);
    assert(U32(h, 0x1cu) == 3u && U32(h, 0x20u) == 400u);
    assert(U32(h, 0x2cu) == 226u && U32(h, 0x30u) == 1000u);
    assert(U32(h, 0x4cu) == 2u && U32(h, 0x50u) == 2000u);
    assert(U32(h, 0x13cu) == 4u && U32(h, 0x140u) == 3000u);
    assert(h[0x0b4u] == 0u && h[0x0b4u + 55u] == 55u);

    std::cout << "PASS classic m2 header\n";
    return 0;
}
