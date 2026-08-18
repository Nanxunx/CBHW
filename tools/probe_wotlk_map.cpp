#include "turtle335/adt/WotlkAdtProbe.h"
#include "turtle335/adt/WotlkAdtReader.h"
#include "turtle335/adt/WotlkMapProbe.h"
#include "turtle335/adt/WotlkWdtReader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace turtle335;

namespace {

std::vector<std::uint8_t> ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("cannot open input file: " + path.string());
    const std::streamoff end = in.tellg();
    if (end < 0)
        throw std::runtime_error("cannot determine input size: " + path.string());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    in.seekg(0, std::ios::beg);
    if (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("cannot read complete input file: " + path.string());
    return bytes;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsAdtPath(const fs::path& path)
{
    return Lower(path.extension().string()) == ".adt";
}

std::string LabelFor(const fs::path& root, const fs::path& path)
{
    std::error_code error;
    const fs::path relative = fs::relative(path, root, error);
    return error ? path.filename().generic_string() : relative.generic_string();
}

std::vector<fs::path> DiscoverAdts(const fs::path& root, bool recursive)
{
    if (!fs::exists(root))
        throw std::runtime_error("input directory does not exist: " + root.string());
    if (!fs::is_directory(root))
        throw std::runtime_error("input path is not a directory: " + root.string());

    std::vector<fs::path> paths;
    const fs::directory_options options = fs::directory_options::skip_permission_denied;
    if (recursive)
    {
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root, options))
        {
            std::error_code error;
            if (entry.is_regular_file(error) && !error && IsAdtPath(entry.path()))
                paths.push_back(entry.path());
        }
    }
    else
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(root, options))
        {
            std::error_code error;
            if (entry.is_regular_file(error) && !error && IsAdtPath(entry.path()))
                paths.push_back(entry.path());
        }
    }

    std::sort(paths.begin(), paths.end(), [](const fs::path& left, const fs::path& right) {
        return left.generic_string() < right.generic_string();
    });
    return paths;
}

const char* StatusName(adt::WotlkProbeStatus status) noexcept
{
    switch (status)
    {
        case adt::WotlkProbeStatus::Clean: return "CLEAN";
        case adt::WotlkProbeStatus::Risk: return "RISK";
        case adt::WotlkProbeStatus::Blocker: return "BLOCKER";
        case adt::WotlkProbeStatus::ParseFailure: return "PARSE-FAIL";
    }
    return "UNKNOWN";
}

void Usage(const char* exe)
{
    std::cerr
        << "Usage:\n  " << exe
        << " <adt-directory> [--wdt <source.wdt>] [--recursive] [--details]\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2)
        {
            Usage(argv[0]);
            return 64;
        }

        const fs::path root = fs::absolute(argv[1]);
        fs::path sourceWdt;
        bool recursive = false;
        bool details = false;

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--recursive")
                recursive = true;
            else if (arg == "--details")
                details = true;
            else if (arg == "--wdt" && i + 1 < argc)
                sourceWdt = fs::absolute(argv[++i]);
            else
            {
                Usage(argv[0]);
                throw std::invalid_argument("unknown or incomplete argument: " + arg);
            }
        }

        std::optional<bool> sourceBigAlpha;
        bool globalWmo = false;
        if (!sourceWdt.empty())
        {
            const adt::WotlkWdtDocument wdt = adt::ParseWotlkWdt(ReadFile(sourceWdt));
            sourceBigAlpha = wdt.bigAlpha;
            globalWmo = wdt.globalWmo;
            std::cout << "WDT: MVER=" << wdt.version
                      << " big-alpha=" << (wdt.bigAlpha ? "yes" : "no")
                      << " global-WMO=" << (wdt.globalWmo ? "yes" : "no") << '\n';
        }

        const std::vector<fs::path> paths = DiscoverAdts(root, recursive);
        if (paths.empty())
            throw std::runtime_error("no .adt files found under: " + root.string());

        std::vector<adt::WotlkMapProbeTile> tiles;
        tiles.reserve(paths.size());

        for (const fs::path& path : paths)
        {
            adt::WotlkMapProbeTile tile;
            tile.label = LabelFor(root, path);
            try
            {
                const adt::WotlkAdtDocument source = adt::ParseWotlkAdt(ReadFile(path));
                tile.report = adt::ProbeWotlkAdt(source, sourceBigAlpha);
            }
            catch (const std::exception& error)
            {
                tile.parseError = error.what();
            }
            tiles.push_back(std::move(tile));
        }

        const adt::WotlkMapProbeSummary summary = adt::SummarizeWotlkMapProbeTiles(tiles);

        std::cout
            << "tiles: discovered=" << summary.discoveredTiles
            << " parsed=" << summary.parsedTiles
            << " clean=" << summary.cleanTiles
            << " risk=" << summary.riskTiles
            << " blocker=" << summary.blockerTiles
            << " parse-fail=" << summary.parseFailures << '\n'
            << "catalog: textures=" << summary.textureCatalogEntries
            << " M2=" << summary.m2Placements
            << " WMO=" << summary.wmoPlacements
            << " MH2O-tiles=" << summary.tilesWithMh2o
            << " MFBO-tiles=" << summary.tilesWithMfbo << '\n'
            << "alpha-cells: none=" << summary.cellsNoAlpha
            << " legacy4=" << summary.cellsLegacy4Alpha
            << " big8=" << summary.cellsBig8Alpha
            << " rle8=" << summary.cellsRle8Alpha
            << " mixed=" << summary.cellsMixedAlpha
            << " unknown=" << summary.cellsUnknownAlpha << '\n'
            << "feature-cells: MCSH=" << summary.cellsWithMcsh
            << " MCCV=" << summary.cellsWithMccv
            << " MCSE=" << summary.cellsWithMcse
            << " old-MCLQ=" << summary.cellsWithLegacyMclq
            << " sound-emitters=" << summary.soundEmitterCount << '\n'
            << "risk-cells: high-res-holes=" << summary.cellsHighResolutionHoles
            << " field3E=" << summary.cellsNonzeroField3E
            << " disable-doodads=" << summary.cellsDisableDoodadsMap
            << " tail-dwords=" << summary.cellsNonzeroTailDwords
            << " unverified-flags=" << summary.cellsUnverifiedFlags
            << " invalid-M2-refs=" << summary.invalidM2References
            << " invalid-WMO-refs=" << summary.invalidWmoReferences << '\n';

        if (!summary.liquidTypes.empty())
        {
            std::cout << "MH2O-liquid-types:\n";
            for (const adt::WotlkMapProbeLiquidSummary& liquid : summary.liquidTypes)
            {
                std::cout << "  id=" << liquid.sourceLiquidType
                          << " layers=" << liquid.layerCount
                          << " cells=" << liquid.cellCount
                          << " tiles=" << liquid.tileCount << '\n';
            }
        }

        if (!summary.issues.empty())
        {
            std::cout << "issue-summary:\n";
            for (const adt::WotlkMapProbeIssueSummary& issue : summary.issues)
            {
                std::cout << "  " << (issue.blocker ? "BLOCKER" : "RISK")
                          << " [" << issue.code << "]"
                          << " occurrences=" << issue.occurrences
                          << " tiles=" << issue.tileCount << '\n';
            }
        }

        if (!summary.cleanTileLabels.empty())
        {
            constexpr std::size_t kCandidateLimit = 20;
            std::cout << "clean-candidates (first "
                      << std::min(kCandidateLimit, summary.cleanTileLabels.size()) << "):\n";
            for (std::size_t i = 0; i < summary.cleanTileLabels.size() && i < kCandidateLimit; ++i)
                std::cout << "  " << summary.cleanTileLabels[i] << '\n';
        }

        if (details)
        {
            std::cout << "tile-details:\n";
            for (const adt::WotlkMapProbeTile& tile : tiles)
            {
                const adt::WotlkProbeStatus status = adt::ClassifyWotlkMapProbeTile(tile);
                std::cout << "  " << StatusName(status) << " " << tile.label;
                if (!tile.report.has_value())
                {
                    std::cout << " error=" << tile.parseError << '\n';
                    continue;
                }
                std::cout << " issues=" << tile.report->issues.size()
                          << " textures=" << tile.report->textureCount
                          << " M2=" << tile.report->m2PlacementCount
                          << " WMO=" << tile.report->wmoPlacementCount << '\n';
                for (const adt::WotlkAdtProbeIssue& issue : tile.report->issues)
                {
                    std::cout << "    " << (issue.blocker ? "BLOCKER" : "RISK")
                              << " [" << issue.code << ']';
                    if (issue.cellSlot != static_cast<std::size_t>(-1))
                        std::cout << " MCNK-slot=" << issue.cellSlot;
                    std::cout << ": " << issue.message << '\n';
                }
            }
        }

        if (globalWmo)
        {
            std::cerr << "BLOCKER [GlobalWmoWdt]: supplied WDT is global-WMO; terrain ADT conversion profile does not apply.\n";
            return 2;
        }
        if (summary.parseFailures != 0 || summary.blockerTiles != 0)
            return 2;
        if (summary.riskTiles != 0)
            return 3;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
