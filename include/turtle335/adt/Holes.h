#pragma once

#include <cstdint>

namespace turtle335::adt {

// WoW 3.3.5a build 12340 and Vanilla/Turtle use the same 16-bit
// 4x4 coarse hole mask for an 8x8 terrain-quad MCNK.
constexpr bool IsQuadHole(std::uint16_t coarseMask, int x, int y) noexcept
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8)
        return false;

    const int bit = (y >> 1) * 4 + (x >> 1);
    return (coarseMask & (std::uint16_t{1} << bit)) != 0;
}

} // namespace turtle335::adt
