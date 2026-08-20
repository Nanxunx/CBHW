#include "turtle335/m2/QuaternionCodec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace turtle335::m2 {
namespace {

std::int16_t ReadI16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error("compressed quaternion key is truncated");
    const std::uint16_t u = static_cast<std::uint16_t>(d[o]) |
                            static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
    return static_cast<std::int16_t>(u);
}

void AppendF32(std::vector<std::uint8_t>& out, const float value)
{
    std::uint32_t bits = 0u;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    out.push_back(static_cast<std::uint8_t>(bits & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 8u) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 16u) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((bits >> 24u) & 0xffu));
}

std::size_t QuaternionMultiplicity(const std::int16_t interpolationType) noexcept
{
    return (interpolationType == 2 || interpolationType == 3) ? 3u : 1u;
}

void AppendIdentity(std::vector<std::uint8_t>& out)
{
    AppendF32(out, 0.0f);
    AppendF32(out, 0.0f);
    AppendF32(out, 0.0f);
    AppendF32(out, 1.0f);
}

} // namespace

float DecodeWotlkQuaternionComponent(const std::int16_t value) noexcept
{
    // Golden compatibility: the compressed -1 representation must become
    // an exact IEEE754 +1.0f in the target, avoiding the historical
    // 0.999969... drift seen in generic formulas.
    if (value == static_cast<std::int16_t>(-1))
        return 1.0f;

    constexpr float divisor = 32767.0f;
    const std::int32_t expanded = value > 0
        ? static_cast<std::int32_t>(value) - 32767
        : static_cast<std::int32_t>(value) + 32767;
    return static_cast<float>(expanded) / divisor;
}

std::vector<std::uint8_t> ExpandWotlkQuaternionKey(
    const std::vector<std::uint8_t>& sourceKey,
    const std::int16_t interpolationType)
{
    const std::size_t multiplicity = QuaternionMultiplicity(interpolationType);
    const std::size_t expected = multiplicity * 8u;
    if (sourceKey.size() != expected)
        throw std::runtime_error("compressed quaternion key has unexpected physical size");

    std::vector<std::uint8_t> out;
    out.reserve(multiplicity * 16u);
    for (std::size_t q = 0u; q < multiplicity; ++q)
    {
        const std::size_t base = q * 8u;
        for (std::size_t component = 0u; component < 4u; ++component)
        {
            AppendF32(out, DecodeWotlkQuaternionComponent(
                ReadI16(sourceKey, base + component * 2u)));
        }
    }
    return out;
}

std::vector<std::uint8_t> BuildClassicIdentityQuaternionKey(
    const std::int16_t interpolationType)
{
    const std::size_t multiplicity = QuaternionMultiplicity(interpolationType);
    std::vector<std::uint8_t> out;
    out.reserve(multiplicity * 16u);
    for (std::size_t i = 0u; i < multiplicity; ++i)
        AppendIdentity(out);
    return out;
}

} // namespace turtle335::m2
