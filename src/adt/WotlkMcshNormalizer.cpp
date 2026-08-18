#include "turtle335/adt/WotlkMcshNormalizer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace turtle335::adt {
namespace {

constexpr std::size_t kPayloadSize = 64u * 8u;

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool GetBit(const std::vector<std::uint8_t>& payload, std::size_t x, std::size_t y)
{
    const std::size_t bit = y * 64u + x;
    return ((payload[bit / 8u] >> (bit % 8u)) & 1u) != 0;
}

void SetBit(std::vector<std::uint8_t>& payload, std::size_t x, std::size_t y, bool value)
{
    const std::size_t bit = y * 64u + x;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (bit % 8u));
    if (value)
        payload[bit / 8u] |= mask;
    else
        payload[bit / 8u] &= static_cast<std::uint8_t>(~mask);
}

} // namespace

std::vector<std::uint8_t> NormalizeWotlkMcsh(const std::vector<std::uint8_t>& rawMcsh,
                                             bool sourceDoNotFixAlphaMap)
{
    if (rawMcsh.empty())
        return {};
    if (rawMcsh.size() != 8u + kPayloadSize)
        throw std::runtime_error("WotLK MCSH must contain exactly 512 payload bytes");
    if (std::memcmp(rawMcsh.data(), "HSCM", 4) != 0)
        throw std::runtime_error("WotLK MCSH raw FourCC mismatch");
    if (ReadU32(rawMcsh, 4) != kPayloadSize)
        throw std::runtime_error("WotLK MCSH declared payload size must be 512");

    std::vector<std::uint8_t> payload(rawMcsh.begin() + 8, rawMcsh.end());

    if (!sourceDoNotFixAlphaMap)
    {
        // Noggit's loader treats the 512 bytes as 64 uint64 rows. On little-
        // endian clients bit N of each row is column N, so this byte-level
        // implementation is endian-independent but semantically identical.
        for (std::size_t row = 0; row < 64; ++row)
            SetBit(payload, 63, row, GetBit(payload, 62, row));
        for (std::size_t column = 0; column < 64; ++column)
            SetBit(payload, column, 63, GetBit(payload, column, 62));
        SetBit(payload, 63, 63, GetBit(payload, 62, 62));
    }

    if (std::all_of(payload.begin(), payload.end(), [](std::uint8_t value) { return value == 0; }))
        return {};

    std::vector<std::uint8_t> result(8u + kPayloadSize, 0);
    std::memcpy(result.data(), "HSCM", 4);
    result[4] = 0x00;
    result[5] = 0x02;
    result[6] = 0x00;
    result[7] = 0x00;
    std::copy(payload.begin(), payload.end(), result.begin() + 8);
    return result;
}

} // namespace turtle335::adt
