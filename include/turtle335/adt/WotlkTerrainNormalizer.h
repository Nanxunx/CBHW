#pragma once

#include "turtle335/adt/TerrainWriter.h"
#include "turtle335/adt/WotlkAdtReader.h"

#include <cstddef>

namespace turtle335::adt {

enum class WotlkAlphaStorage
{
    None,
    Legacy4Sequential,
    Big8Independent,
    Rle8Independent
};

struct WotlkTerrainNormalizationResult
{
    TerrainCellInput terrain;
    WotlkAlphaStorage alphaStorage = WotlkAlphaStorage::None;
};

// Converts one build-12340 MCNK's terrain payload into version-neutral semantic
// terrain. sourceWdtBigAlpha is retained as a consistency hint only; per-layer
// physical size/flags are authoritative, matching Noggit's robust loader.
WotlkTerrainNormalizationResult NormalizeWotlkTerrain(const WotlkMcnkRecord& cell,
                                                       std::size_t sourceTextureCount,
                                                       bool sourceWdtBigAlpha = false);

} // namespace turtle335::adt
