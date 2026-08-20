#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace turtle335::adt {

using Alpha8 = std::array<std::uint8_t, 64 * 64>;
using Alpha4Packed = std::array<std::uint8_t, (64 * 64) / 2>;

std::uint8_t Expand4To8(std::uint8_t nibble) noexcept;
std::uint8_t Quantize8To4(std::uint8_t alpha) noexcept;

Alpha8 DecodeLegacyAlpha4(const std::uint8_t* data, std::size_t size);
Alpha8 DecodeBigAlpha8(const std::uint8_t* data, std::size_t size);
Alpha8 DecodeRleAlpha8(const std::uint8_t* data, std::size_t size);
Alpha4Packed EncodeLegacyAlpha4(const Alpha8& alpha) noexcept;

} // namespace turtle335::adt
