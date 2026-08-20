#pragma once

#include "turtle335/adt/WotlkAdtProbe.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace turtle335::adt {

enum class WotlkProbeStatus
{
    Clean,
    Risk,
    Blocker,
    ParseFailure
};

struct WotlkMapProbeTile
{
    std::string label;
    std::optional<WotlkAdtProbeReport> report;
    std::string parseError;
};

struct WotlkMapProbeIssueSummary
{
    bool blocker = false;
    std::string code;
    std::size_t occurrences = 0;
    std::size_t tileCount = 0;
};

struct WotlkMapProbeLiquidSummary
{
    std::uint16_t sourceLiquidType = 0;
    std::size_t layerCount = 0;
    std::size_t cellCount = 0;
    std::size_t tileCount = 0;
};

struct WotlkMapProbeSummary
{
    std::size_t discoveredTiles = 0;
    std::size_t parsedTiles = 0;
    std::size_t cleanTiles = 0;
    std::size_t riskTiles = 0;
    std::size_t blockerTiles = 0;
    std::size_t parseFailures = 0;

    std::size_t tilesWithMh2o = 0;
    std::size_t tilesWithMfbo = 0;
    std::size_t textureCatalogEntries = 0;
    std::size_t m2Placements = 0;
    std::size_t wmoPlacements = 0;

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

    std::vector<WotlkMapProbeIssueSummary> issues;
    std::vector<WotlkMapProbeLiquidSummary> liquidTypes;
    std::vector<std::string> cleanTileLabels;
};

WotlkProbeStatus ClassifyWotlkMapProbeTile(const WotlkMapProbeTile& tile) noexcept;
WotlkMapProbeSummary SummarizeWotlkMapProbeTiles(const std::vector<WotlkMapProbeTile>& tiles);

} // namespace turtle335::adt
