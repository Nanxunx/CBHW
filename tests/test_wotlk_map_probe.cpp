#include "turtle335/adt/WotlkMapProbe.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace turtle335::adt;

static WotlkAdtProbeReport CleanReport()
{
    WotlkAdtProbeReport report;
    report.version = 18;
    report.textureCount = 4;
    report.m2PlacementCount = 10;
    report.wmoPlacementCount = 2;
    report.cellsNoAlpha = 200;
    report.cellsLegacy4Alpha = 56;
    report.cellsWithMcsh = 3;
    return report;
}

int main()
{
    std::vector<WotlkMapProbeTile> tiles;

    WotlkMapProbeTile clean;
    clean.label = "Map_30_30.adt";
    clean.report = CleanReport();
    clean.report->hasMh2o = true;
    clean.report->liquidTypes.push_back({7, 2, 2});
    tiles.push_back(clean);

    WotlkMapProbeTile risk;
    risk.label = "Map_31_30.adt";
    risk.report = CleanReport();
    risk.report->cellsWithMccv = 5;
    risk.report->hasMfbo = true;
    risk.report->issues.push_back({false, 0, "TargetMccvLoss", "MCCV target path is not approved"});
    risk.report->issues.push_back({false, static_cast<std::size_t>(-1), "TargetMfboLoss", "MFBO not normalized"});
    risk.report->liquidTypes.push_back({7, 1, 1});
    risk.report->liquidTypes.push_back({9, 3, 2});
    tiles.push_back(risk);

    WotlkMapProbeTile blocker;
    blocker.label = "Map_32_30.adt";
    blocker.report = CleanReport();
    blocker.report->cellsHighResolutionHoles = 1;
    blocker.report->issues.push_back({true, 6, "HighResolutionHoles", "unsupported hole encoding"});
    blocker.report->issues.push_back({false, 1, "TargetMccvLoss", "another risk"});
    tiles.push_back(blocker);

    WotlkMapProbeTile failed;
    failed.label = "Map_33_30.adt";
    failed.parseError = "truncated MCIN";
    tiles.push_back(failed);

    assert(ClassifyWotlkMapProbeTile(tiles[0]) == WotlkProbeStatus::Clean);
    assert(ClassifyWotlkMapProbeTile(tiles[1]) == WotlkProbeStatus::Risk);
    assert(ClassifyWotlkMapProbeTile(tiles[2]) == WotlkProbeStatus::Blocker);
    assert(ClassifyWotlkMapProbeTile(tiles[3]) == WotlkProbeStatus::ParseFailure);

    const WotlkMapProbeSummary summary = SummarizeWotlkMapProbeTiles(tiles);
    assert(summary.discoveredTiles == 4);
    assert(summary.parsedTiles == 3);
    assert(summary.cleanTiles == 1);
    assert(summary.riskTiles == 1);
    assert(summary.blockerTiles == 1);
    assert(summary.parseFailures == 1);

    assert(summary.tilesWithMh2o == 1);
    assert(summary.tilesWithMfbo == 1);
    assert(summary.textureCatalogEntries == 12);
    assert(summary.m2Placements == 30);
    assert(summary.wmoPlacements == 6);
    assert(summary.cellsNoAlpha == 600);
    assert(summary.cellsLegacy4Alpha == 168);
    assert(summary.cellsWithMcsh == 9);
    assert(summary.cellsWithMccv == 5);
    assert(summary.cellsHighResolutionHoles == 1);

    assert(summary.cleanTileLabels.size() == 1);
    assert(summary.cleanTileLabels[0] == "Map_30_30.adt");

    assert(summary.liquidTypes.size() == 2);
    assert(summary.liquidTypes[0].sourceLiquidType == 7);
    assert(summary.liquidTypes[0].layerCount == 3);
    assert(summary.liquidTypes[0].cellCount == 3);
    assert(summary.liquidTypes[0].tileCount == 2);
    assert(summary.liquidTypes[1].sourceLiquidType == 9);
    assert(summary.liquidTypes[1].layerCount == 3);
    assert(summary.liquidTypes[1].cellCount == 2);
    assert(summary.liquidTypes[1].tileCount == 1);

    bool sawMccv = false;
    bool sawMfbo = false;
    bool sawHoles = false;
    for (const WotlkMapProbeIssueSummary& issue : summary.issues)
    {
        if (!issue.blocker && issue.code == "TargetMccvLoss")
        {
            sawMccv = true;
            assert(issue.occurrences == 2);
            assert(issue.tileCount == 2);
        }
        if (!issue.blocker && issue.code == "TargetMfboLoss")
        {
            sawMfbo = true;
            assert(issue.occurrences == 1);
            assert(issue.tileCount == 1);
        }
        if (issue.blocker && issue.code == "HighResolutionHoles")
        {
            sawHoles = true;
            assert(issue.occurrences == 1);
            assert(issue.tileCount == 1);
        }
    }
    assert(sawMccv && sawMfbo && sawHoles);

    std::cout << "turtle335_wotlk_map_probe_tests: OK\n";
    return 0;
}
