#include "turtle335/adt/WotlkAdtProbe.h"
#include "turtle335/adt/WotlkAdtReader.h"
#include "turtle335/adt/WotlkWdtReader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
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

void Usage(const char* exe)
{
    std::cerr << "Usage:\n  " << exe << " <source.adt> [--wdt <source.wdt>]\n";
}

const char* YesNo(bool value) noexcept
{
    return value ? "yes" : "no";
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

        const fs::path sourceAdt = fs::absolute(argv[1]);
        fs::path sourceWdt;
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--wdt" && i + 1 < argc)
                sourceWdt = fs::absolute(argv[++i]);
            else
            {
                Usage(argv[0]);
                throw std::invalid_argument("unknown or incomplete argument: " + arg);
            }
        }

        std::optional<bool> sourceBigAlpha;
        if (!sourceWdt.empty())
        {
            const adt::WotlkWdtDocument wdt = adt::ParseWotlkWdt(ReadFile(sourceWdt));
            sourceBigAlpha = wdt.bigAlpha;
            std::cout << "WDT: MVER=" << wdt.version
                      << " big-alpha=" << YesNo(wdt.bigAlpha)
                      << " global-WMO=" << YesNo(wdt.globalWmo) << '\n';
        }

        const adt::WotlkAdtDocument source = adt::ParseWotlkAdt(ReadFile(sourceAdt));
        const adt::WotlkAdtProbeReport report = adt::ProbeWotlkAdt(source, sourceBigAlpha);

        std::cout
            << "ADT: MVER=" << report.version
            << " textures=" << report.textureCount
            << " M2=" << report.m2PlacementCount
            << " WMO=" << report.wmoPlacementCount
            << " MH2O=" << YesNo(report.hasMh2o)
            << " MFBO=" << YesNo(report.hasMfbo) << '\n'
            << "alpha-cells: none=" << report.cellsNoAlpha
            << " legacy4=" << report.cellsLegacy4Alpha
            << " big8=" << report.cellsBig8Alpha
            << " rle8=" << report.cellsRle8Alpha
            << " mixed=" << report.cellsMixedAlpha
            << " unknown=" << report.cellsUnknownAlpha << '\n'
            << "feature-cells: MCSH=" << report.cellsWithMcsh
            << " MCCV=" << report.cellsWithMccv
            << " MCSE=" << report.cellsWithMcse
            << " old-MCLQ=" << report.cellsWithLegacyMclq
            << " sound-emitters=" << report.soundEmitterCount << '\n'
            << "risk-cells: high-res-holes=" << report.cellsHighResolutionHoles
            << " field3E=" << report.cellsNonzeroField3E
            << " disable-doodads=" << report.cellsDisableDoodadsMap
            << " tail-dwords=" << report.cellsNonzeroTailDwords
            << " unverified-flags=" << report.cellsUnverifiedFlags
            << " invalid-M2-refs=" << report.invalidM2References
            << " invalid-WMO-refs=" << report.invalidWmoReferences << '\n';

        if (report.hasMh2o)
        {
            std::cout << "MH2O-liquid-types:";
            if (!report.mh2oParsed)
                std::cout << " parse-failed";
            else if (report.liquidTypes.empty())
                std::cout << " none";
            else
            {
                for (const adt::WotlkLiquidTypeUsage& usage : report.liquidTypes)
                {
                    std::cout << " id=" << usage.sourceLiquidType
                              << "(layers=" << usage.layerCount
                              << ",cells=" << usage.cellCount << ')';
                }
            }
            std::cout << '\n';
        }

        bool blocker = false;
        bool warning = false;
        for (const adt::WotlkAdtProbeIssue& issue : report.issues)
        {
            blocker = blocker || issue.blocker;
            warning = warning || !issue.blocker;
            std::cerr << (issue.blocker ? "BLOCKER" : "RISK")
                      << " [" << issue.code << ']';
            if (issue.cellSlot != static_cast<std::size_t>(-1))
                std::cerr << " MCNK-slot=" << issue.cellSlot;
            std::cerr << ": " << issue.message << '\n';
        }

        if (blocker)
            return 2;
        if (warning)
            return 3;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
