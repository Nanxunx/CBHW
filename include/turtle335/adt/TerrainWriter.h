#pragma once

#include "turtle335/adt/Mcal.h"
#include "turtle335/adt/McnkWriter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace turtle335::adt {

struct TerrainNormal
{
    float x = 0.0f;
    float y = 1.0f;
    float z = 0.0f;
};

struct TerrainLayerInput
{
    // Index into the top-level MTEX string sequence.
    std::uint32_t textureId = 0;
    // Target-compatible MCLY flags. USE_ALPHA/COMPRESSED are writer-owned.
    std::uint32_t flags = 0;
    std::uint32_t effectId = 0xFFFFu;
    // Normalized WotLK/big-alpha contribution. Required for every non-base layer.
    std::optional<Alpha8> alpha;
};

struct TerrainCellInput
{
    // Absolute vertical heights in ADT's 145-vertex order.
    std::array<float, 145> heights{};
    std::array<TerrainNormal, 145> normals{};
    std::vector<TerrainLayerInput> layers;
};

struct SerializedTerrain
{
    float baseHeight = 0.0f;
    std::uint32_t nLayers = 0;
    std::vector<std::uint8_t> mcvt;
    std::vector<std::uint8_t> mcnr;
    std::vector<std::uint8_t> mcly;
    // Canonical writer emits MCAL even with zero payload for a one-layer chunk.
    std::vector<std::uint8_t> mcal;
};

SerializedTerrain SerializeLegacyTerrain(const TerrainCellInput& input,
                                         std::size_t textureCount);

// Applies the generated terrain chunks to one MCNK target record.
// The caller remains responsible for ix/iy, horizontal position, area, holes,
// placement refs, liquids, shadows and optional vertex colors.
void ApplyTerrainToMcnk(const SerializedTerrain& terrain,
                        McnkTargetHeader& header,
                        McnkSubchunks& subchunks);

} // namespace turtle335::adt
