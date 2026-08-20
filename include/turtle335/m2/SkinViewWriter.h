#pragma once

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

constexpr std::uint32_t kWotlkSkinHeaderSize = 48u;
constexpr std::uint32_t kClassicViewHeaderSize = 44u;
constexpr std::uint32_t kWotlkSkinSubmeshSize = 48u;
constexpr std::uint32_t kClassicViewSubmeshSize = 32u;
constexpr std::uint32_t kSkinTextureUnitSize = 24u;

// Convert one or more WotLK SKIN files into the embedded Classic/Turtle View
// block. Returned offsets are absolute M2 offsets based on targetAbsoluteOffset.
std::vector<std::uint8_t> BuildClassicEmbeddedViews(
    const std::vector<std::vector<std::uint8_t>>& skinFiles,
    std::uint32_t targetAbsoluteOffset);

} // namespace turtle335::m2
