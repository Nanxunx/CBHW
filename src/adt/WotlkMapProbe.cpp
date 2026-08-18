#include "turtle335/adt/WotlkMapProbe.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace turtle335::adt {
namespace {

struct IssueKey
{
    bool blocker = false;
    std::string code;

    bool operator<(const IssueKey& other) const noexcept
    {
        if (blocker != other.blocker)
            return blocker > other.blocker; // blockers first in final sorted map
        return code < other.code;
    }
};

struct IssueAccumulator
{
    std::size_t occurrences = 0;
    std::set<std::string> tiles;
};

struct LiquidAccumulator
{
    std::size_t layerCount = 0;
    std::size_t cellCount = 0;
    std::set<std::string> tiles;
};

} // namespace

WotlkProbeStatus ClassifyWotlkMapProbeTile(const WotlkMapProbeTile& tile) noexcept
{
    if (!tile.report.has_value())
        return WotlkProbeStatus::ParseFailure;

    bool risk = false;
    for (const WotlkAdtProbeIssue& issue : tile.report->issues)
    {
        if (issue.blocker)
            return WotlkProbeStatus::Blocker;
        risk = true;
    }
    return risk ? WotlkProbeStatus::Risk : WotlkProbeStatus::Clean;
}

WotlkMapProbeSummary SummarizeWotlkMapProbeTiles(const std::vector<WotlkMapProbeTile>& tiles)
{
    WotlkMapProbeSummary summary;
    summary.discoveredTiles = tiles.size();

    std::map<IssueKey, IssueAccumulator> issueMap;
    std::map<std::uint16_t, LiquidAccumulator> liquidMap;

    for (const WotlkMapProbeTile& tile : tiles)
    {
        const WotlkProbeStatus status = ClassifyWotlkMapProbeTile(tile);
        switch (status)
        {
            case WotlkProbeStatus::Clean:
                ++summary.cleanTiles;
                summary.cleanTileLabels.push_back(tile.label);
                break;
            case WotlkProbeStatus::Risk:
                ++summary.riskTiles;
                break;
            case WotlkProbeStatus::Blocker:
                ++summary.blockerTiles;
                break;
            case WotlkProbeStatus::ParseFailure:
                ++summary.parseFailures;
                break;
        }

        if (!tile.report.has_value())
            continue;

        ++summary.parsedTiles;
        const WotlkAdtProbeReport& report = *tile.report;
        summary.tilesWithMh2o += report.hasMh2o ? 1u : 0u;
        summary.tilesWithMfbo += report.hasMfbo ? 1u : 0u;
        summary.textureCatalogEntries += report.textureCount;
        summary.m2Placements += report.m2PlacementCount;
        summary.wmoPlacements += report.wmoPlacementCount;

        summary.cellsNoAlpha += report.cellsNoAlpha;
        summary.cellsLegacy4Alpha += report.cellsLegacy4Alpha;
        summary.cellsBig8Alpha += report.cellsBig8Alpha;
        summary.cellsRle8Alpha += report.cellsRle8Alpha;
        summary.cellsMixedAlpha += report.cellsMixedAlpha;
        summary.cellsUnknownAlpha += report.cellsUnknownAlpha;

        summary.cellsWithMcsh += report.cellsWithMcsh;
        summary.cellsWithMccv += report.cellsWithMccv;
        summary.cellsWithMcse += report.cellsWithMcse;
        summary.cellsWithLegacyMclq += report.cellsWithLegacyMclq;
        summary.soundEmitterCount += report.soundEmitterCount;

        summary.cellsHighResolutionHoles += report.cellsHighResolutionHoles;
        summary.cellsNonzeroField3E += report.cellsNonzeroField3E;
        summary.cellsDisableDoodadsMap += report.cellsDisableDoodadsMap;
        summary.cellsNonzeroTailDwords += report.cellsNonzeroTailDwords;
        summary.cellsUnverifiedFlags += report.cellsUnverifiedFlags;
        summary.invalidM2References += report.invalidM2References;
        summary.invalidWmoReferences += report.invalidWmoReferences;

        for (const WotlkAdtProbeIssue& issue : report.issues)
        {
            IssueAccumulator& accumulator = issueMap[{issue.blocker, issue.code}];
            ++accumulator.occurrences;
            accumulator.tiles.insert(tile.label);
        }

        for (const WotlkLiquidTypeUsage& usage : report.liquidTypes)
        {
            LiquidAccumulator& accumulator = liquidMap[usage.sourceLiquidType];
            accumulator.layerCount += usage.layerCount;
            accumulator.cellCount += usage.cellCount;
            accumulator.tiles.insert(tile.label);
        }
    }

    std::sort(summary.cleanTileLabels.begin(), summary.cleanTileLabels.end());

    summary.issues.reserve(issueMap.size());
    for (const auto& entry : issueMap)
    {
        WotlkMapProbeIssueSummary item;
        item.blocker = entry.first.blocker;
        item.code = entry.first.code;
        item.occurrences = entry.second.occurrences;
        item.tileCount = entry.second.tiles.size();
        summary.issues.push_back(std::move(item));
    }

    summary.liquidTypes.reserve(liquidMap.size());
    for (const auto& entry : liquidMap)
    {
        WotlkMapProbeLiquidSummary item;
        item.sourceLiquidType = entry.first;
        item.layerCount = entry.second.layerCount;
        item.cellCount = entry.second.cellCount;
        item.tileCount = entry.second.tiles.size();
        summary.liquidTypes.push_back(item);
    }

    return summary;
}

} // namespace turtle335::adt
