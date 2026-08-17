#pragma once

#include "turtle335/adt/LegacyLiquid.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace turtle335::adt {

struct McnkTargetHeader
{
    std::uint32_t flags = 0;
    std::uint32_t ix = 0;
    std::uint32_t iy = 0;
    std::uint32_t nLayers = 0;
    std::uint32_t nDoodadRefs = 0;
    std::uint32_t areaId = 0;
    std::uint32_t nMapObjRefs = 0;
    std::uint16_t holes = 0;
    std::uint16_t legacy3E = 0;

    std::array<std::uint8_t, 16> lowQualityTextureMap{};

    std::uint32_t predTex = 0;
    std::uint32_t nEffectDoodad = 0;
    std::uint32_t nSndEmitters = 0;

    float z = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    std::uint32_t props = 0;
    std::uint32_t effectId = 0;
};

struct McnkSubchunks
{
    // Complete raw ADT subchunks: reversed on-disk FourCC + size + payload.
    std::vector<std::uint8_t> mcvt;
    std::vector<std::uint8_t> mcnr;
    std::vector<std::uint8_t> mcly;
    std::vector<std::uint8_t> mcrf;
    std::vector<std::uint8_t> mcsh;
    std::vector<std::uint8_t> mcal;
    std::vector<std::uint8_t> mcse;
    std::vector<std::uint8_t> mccv;
    std::optional<LegacyMclq> mclq;
};

struct McnkLayout
{
    std::uint32_t offsMCVT = 0;
    std::uint32_t offsMCNR = 0;
    std::uint32_t offsMCLY = 0;
    std::uint32_t offsMCRF = 0;
    std::uint32_t offsMCAL = 0;
    std::uint32_t sizeMCAL = 0;
    std::uint32_t offsMCSH = 0;
    std::uint32_t sizeMCSH = 0;
    std::uint32_t offsMCSE = 0;
    std::uint32_t offsMCLQ = 0;
    std::uint32_t sizeMCLQ = 0;
    std::uint32_t offsMCCV = 0;
};

struct SerializedMcnk
{
    std::vector<std::uint8_t> bytes;
    McnkLayout layout;
};

// Builds one complete raw MCNK chunk. Physical FourCC: KNCM.
// offs* are relative to the first byte of KNCM and point to subchunk headers.
SerializedMcnk SerializeVanillaMcnk(const McnkTargetHeader& header, const McnkSubchunks& subchunks);

} // namespace turtle335::adt
