#pragma once

#include "turtle335/adt/PlacementWriter.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace turtle335::adt {

struct WotlkMcnkHeader
{
    std::uint32_t flags = 0;
    std::uint32_t ix = 0;
    std::uint32_t iy = 0;
    std::uint32_t nLayers = 0;
    std::uint32_t nDoodadRefs = 0;
    std::uint32_t areaId = 0;
    std::uint32_t nMapObjRefs = 0;

    // Build 12340 stores a four-byte region here. The real client consumes
    // the low 16 bits as the 4x4 coarse hole mask; the upper 16 bits are kept
    // separately rather than being misinterpreted as 16 extra hole bits.
    std::uint16_t holes = 0;
    std::uint16_t legacy3E = 0;

    std::array<std::uint8_t, 16> lowQualityTextureMap{};

    // WotLK/Noggit interpretation of the following eight raw bytes. Do not
    // alias-copy these into Vanilla predTex/nEffectDoodad: target semantics are
    // different and require an explicit downgrade policy.
    std::array<std::uint8_t, 8> disableDoodadsMap{};

    std::uint32_t nSndEmitters = 0;

    float z = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    std::uint32_t props = 0;
    std::uint32_t effectId = 0;
};

struct WotlkMcnkRecord
{
    WotlkMcnkHeader header;
    std::vector<std::uint32_t> m2Refs;
    std::vector<std::uint32_t> wmoRefs;

    // Complete raw source subchunks (reversed on-disk FourCC + size + payload).
    // MCNR's historical 13-byte tail is deliberately not included here; later
    // normalization reads the declared semantic normal payload only.
    std::vector<std::uint8_t> mcvt;
    std::vector<std::uint8_t> mcnr;
    std::vector<std::uint8_t> mcly;
    std::vector<std::uint8_t> mcal;
    std::vector<std::uint8_t> mcsh;
    std::vector<std::uint8_t> mcse;
    std::vector<std::uint8_t> mclq;
    std::vector<std::uint8_t> mccv;
};

struct WotlkAdtDocument
{
    std::uint32_t version = 0;
    std::vector<std::string> textures;
    std::vector<M2PlacementInput> m2Placements;
    std::vector<WmoPlacementInput> wmoPlacements;
    std::array<WotlkMcnkRecord, 256> cells;

    // Complete optional top-level source chunks for later semantic conversion.
    std::vector<std::uint8_t> mh2o;
    std::vector<std::uint8_t> mfbo;
};

// Safe structural reader for monolithic WoW 3.3.5a build-12340 ADTs. It does
// not perform retroport conversion; all source offsets are validated and then
// discarded in favor of explicit records/raw semantic subchunks.
WotlkAdtDocument ParseWotlkAdt(const std::vector<std::uint8_t>& bytes);

} // namespace turtle335::adt
