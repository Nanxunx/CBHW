#pragma once

#include "turtle335/adt/AdtWriter.h"
#include "turtle335/adt/LegacyLiquid.h"
#include "turtle335/adt/TerrainWriter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace turtle335::adt {

// Semantic MCNK input shared by source readers and the Vanilla/Turtle writer.
// It deliberately contains no source/target binary offsets or FourCC-relative
// pointers: those belong to the format-specific reader/writer layers.
struct NormalizedAdtCell
{
    std::uint32_t ix = 0;
    std::uint32_t iy = 0;

    std::uint32_t flags = 0;
    std::uint32_t areaId = 0;
    std::uint16_t holes = 0;
    std::uint16_t legacy3E = 0;
    std::array<std::uint8_t, 16> lowQualityTextureMap{};
    std::uint32_t predTex = 0;
    std::uint32_t nEffectDoodad = 0;
    std::uint32_t props = 0;
    std::uint32_t effectId = 0;

    // Horizontal MCNK placement. Vertical baseline is derived from terrain.heights[0].
    float positionX = 0.0f;
    float positionZ = 0.0f;

    TerrainCellInput terrain;
    std::vector<LiquidLayer> liquids;

    // Indices into NormalizedAdt::m2Placements / wmoPlacements.
    std::vector<std::uint32_t> m2Refs;
    std::vector<std::uint32_t> wmoRefs;

    // Optional chunks that already use verified target encodings. These stay
    // explicitly target-prefixed so source readers cannot accidentally copy a
    // WotLK chunk into the normalized semantic model and call it portable.
    std::vector<std::uint8_t> targetMcsh;
    std::vector<std::uint8_t> targetMcse;

    // No targetMccv field on purpose. Turtle's real MCNK pointer-fixup does not
    // process offsMCCV, so WotLK MCCV remains a documented source loss until a
    // separate target-client path is proven by binary/fixture evidence.
};

struct NormalizedAdt
{
    // MTEX order is semantic because TerrainLayerInput::textureId indexes it.
    std::vector<std::string> textures;
    std::vector<M2PlacementInput> m2Placements;
    std::vector<WmoPlacementInput> wmoPlacements;
    std::array<NormalizedAdtCell, 256> cells;

    // Optional complete target-compatible MFBO chunk. MFBO conversion itself
    // remains a separate semantic task and is not silently copied by readers.
    std::vector<std::uint8_t> targetMfbo;
};

struct NormalizedAdtDiagnostic
{
    std::size_t cellSlot = 0;
    LiquidDiagnostic liquid;
};

struct NormalizedAdtBuildResult
{
    SerializedAdt adt;
    bool lossless = true;
    std::vector<NormalizedAdtDiagnostic> diagnostics;
};

// Converts semantic terrain/liquid/placement data into the lower-level writer
// contract. Binary offsets and target chunk layouts are created only after this
// boundary.
AdtWriterInput BuildAdtWriterInput(const NormalizedAdt& input,
                                   bool* lossless = nullptr,
                                   std::vector<NormalizedAdtDiagnostic>* diagnostics = nullptr);

// High-level canonical target entry point used by future 3.3.5 readers.
NormalizedAdtBuildResult SerializeNormalizedAdt(const NormalizedAdt& input);

} // namespace turtle335::adt
