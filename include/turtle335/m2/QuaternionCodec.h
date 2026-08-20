#pragma once

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

// Decode one WotLK/TBC compressed quaternion component to the float component
// used by the Classic/Turtle v256 writer. The source -1 case is intentionally
// exact +1.0f; this is a real Golden compatibility rule, not a generic
// normalization shortcut.
float DecodeWotlkQuaternionComponent(std::int16_t value) noexcept;

// Expand one physical WotLK quaternion key. Interpolation 0/1 uses one
// compressed quaternion (8 -> 16 bytes). Hermite/Bezier interpolation 2/3
// stores value/in-tangent/out-tangent and therefore expands 24 -> 48 bytes.
std::vector<std::uint8_t> ExpandWotlkQuaternionKey(
    const std::vector<std::uint8_t>& sourceKey,
    std::int16_t interpolationType);

// Classic identity quaternion default for a missing per-sequence key. Spline
// tracks receive the identity value for value/in-tangent/out-tangent.
std::vector<std::uint8_t> BuildClassicIdentityQuaternionKey(
    std::int16_t interpolationType);

} // namespace turtle335::m2
