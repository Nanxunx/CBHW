#pragma once

#include "turtle335/adt/LegacyLiquid.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace turtle335::adt {

enum class Mh2oVertexFormat : std::uint16_t
{
    HeightDepth = 0,
    HeightTextureCoord = 1,
    Depth = 2
};

struct Mh2oLayerMetadata
{
    std::uint16_t sourceLiquidType = 0;
    Mh2oVertexFormat sourceVertexFormat = Mh2oVertexFormat::HeightDepth;
    float minHeightLevel = 0.0f;
    float maxHeightLevel = 0.0f;
};

struct ParsedMh2oLayer
{
    LiquidLayer layer;
    Mh2oLayerMetadata metadata;
};

struct Mh2oParseResult
{
    std::array<std::vector<ParsedMh2oLayer>, 256> chunks;
};

using LiquidTypeResolver = std::function<LiquidCategory(std::uint16_t)>;

// Parses one complete MH2O chunk including the 8-byte FourCC/size header.
// Offsets inside MH2O are interpreted relative to the payload start, matching
// the build-12340 TC/AZ extractor convention (chunk start + 8 + offset).
Mh2oParseResult ParseMh2oChunk(const std::uint8_t* data, std::size_t size, const LiquidTypeResolver& resolver);

} // namespace turtle335::adt
