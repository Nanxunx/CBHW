#pragma once

#include "turtle335/adt/WdtWriter.h"

#include <array>
#include <cstdint>
#include <vector>

namespace turtle335::adt {

struct WotlkWdtDocument
{
    std::uint32_t version = 0;
    std::array<std::uint32_t, 8> mphd{};
    std::array<WdtTileEntry, 64 * 64> tiles{};
    bool globalWmo = false;
    bool bigAlpha = false;
};

// Parses the build-12340 WDT root while safely skipping additional chunks.
// Required logical chunks are MVER, MPHD and MAIN; physical FourCC bytes are
// REVM, DHPM and NIAM.
WotlkWdtDocument ParseWotlkWdt(const std::vector<std::uint8_t>& bytes);

// Terrain-only target normalization. WotLK MPHD feature bits are deliberately
// not copied; MAIN presence is preserved, async IDs are canonicalized to zero.
WdtWriterInput NormalizeWotlkTerrainWdt(const WotlkWdtDocument& source);

} // namespace turtle335::adt
