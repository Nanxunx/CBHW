#pragma once

#include "turtle335/adt/WotlkAdtReader.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace turtle335::adt {

enum class WotlkAlphaEncoding
{
    None,
    Legacy4,
    Big8,
    Rle8,
    Mixed,
    Unknown
};

struct WotlkLiquidTypeUsage
{
    std::uint16_t sourceLiquidType = 0;
    std::size_t layerCount = 0;
    std::size_t cellCount = 0;
};

struct WotlkAdtProbeIssue
{
    bool blocker = false;
    std::size_t cellSlot = static_cast<std::size_t>(-1);
    std::string code;
    std::string message;
};

struct WotlkAdtProbeReport
{
    std::uint32_t version = 0;
    std::size_t textureCount = 0;
    std::size_t m2PlacementCount = 0;
    std::size_t wmoPlacementCount = 0;

    bool hasMh2o = false;
    bool hasMfbo = false;
    bool mh2oParsed = true;

    std::size_t cellsNoAlpha = 0;
    std::size_t cellsLegacy4Alpha = 0;
    std::size_t cellsBig8Alpha = 0;
    std::size_t cellsRle8Alpha = 0;
    std::size_t cellsMixedAlpha = 0;
    std::size_t cellsUnknownAlpha = 0;

    std::size_t cellsWithMcsh = 0;
    std::size_t cellsWithMccv = 0;
    std::size_t cellsWithMcse = 0;
    std::size_t cellsWithLegacyMclq = 0;
    std::size_t soundEmitterCount = 0;

    std::size_t cellsHighResolutionHoles = 0;
    std::size_t cellsNonzeroField3E = 0;
    std::size_t cellsDisableDoodadsMap = 0;
    std::size_t cellsNonzeroTailDwords = 0;
    std::size_t cellsUnverifiedFlags = 0;
    std::size_t invalidM2References = 0;
    std::size_t invalidWmoReferences = 0;

    std::vector<WotlkLiquidTypeUsage> liquidTypes;
    std::vector<WotlkAdtProbeIssue> issues;
};

// Source-only inventory/diagnostic pass. It deliberately does not require
// LiquidType.dbc and does not construct target bytes. sourceWdtBigAlpha is an
// optional map-level hint used only to flag suspicious MCAL storage mismatches.
WotlkAdtProbeReport ProbeWotlkAdt(const WotlkAdtDocument& source,
                                  std::optional<bool> sourceWdtBigAlpha = std::nullopt);

} // namespace turtle335::adt
