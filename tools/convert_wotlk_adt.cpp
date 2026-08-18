#include "turtle335/adt/AdtValidator.h"
#include "turtle335/adt/NormalizedAdt.h"
#include "turtle335/adt/WotlkAdtNormalizer.h"
#include "turtle335/adt/WotlkAdtReader.h"
#include "turtle335/adt/WotlkWdtReader.h"
#include "turtle335/dbc/LiquidTypeDbc.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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

void WriteAtomicNewFile(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
    if (fs::exists(path))
        throw std::runtime_error("output already exists; refusing to overwrite: " + path.string());
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());

    const fs::path temporary = path.string() + ".tmp";
    if (fs::exists(temporary))
        fs::remove(temporary);
    try
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot create temporary output: " + temporary.string());
        if (!bytes.empty())
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out)
            throw std::runtime_error("failed while writing temporary output: " + temporary.string());
        fs::rename(temporary, path);
    }
    catch (...)
    {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

const char* SeverityName(adt::WotlkAdtIssueSeverity severity)
{
    return severity == adt::WotlkAdtIssueSeverity::Blocker ? "BLOCKER" : "LOSS";
}

void Usage(const char* exe)
{
    std::cerr
        << "Usage:\n  " << exe
        << " <source.adt> <LiquidType.dbc> <output.adt> [--wdt <source.wdt>] [--allow-lossy]\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4)
        {
            Usage(argv[0]);
            return 64;
        }

        const fs::path sourceAdt = fs::absolute(argv[1]);
        const fs::path liquidDbc = fs::absolute(argv[2]);
        const fs::path outputAdt = fs::absolute(argv[3]);
        fs::path sourceWdt;
        bool allowLossy = false;

        for (int i = 4; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--allow-lossy")
            {
                allowLossy = true;
            }
            else if (arg == "--wdt" && i + 1 < argc)
            {
                sourceWdt = fs::absolute(argv[++i]);
            }
            else
            {
                Usage(argv[0]);
                throw std::invalid_argument("unknown or incomplete argument: " + arg);
            }
        }

        if (sourceAdt == outputAdt)
            throw std::invalid_argument("source and target ADT paths must differ");

        const dbc::LiquidTypeDbc liquidTypes = dbc::ParseLiquidTypeDbc(ReadFile(liquidDbc));
        const adt::LiquidTypeResolver resolver = dbc::MakeLiquidTypeResolver(liquidTypes);

        bool sourceBigAlpha = false;
        if (!sourceWdt.empty())
        {
            const adt::WotlkWdtDocument wdt = adt::ParseWotlkWdt(ReadFile(sourceWdt));
            sourceBigAlpha = wdt.bigAlpha;
            if (wdt.globalWmo)
                throw std::runtime_error("source WDT is global-WMO; terrain ADT conversion profile does not apply");
        }

        const adt::WotlkAdtDocument source = adt::ParseWotlkAdt(ReadFile(sourceAdt));
        const adt::WotlkAdtNormalizationResult normalized =
            adt::NormalizeWotlkAdt(source, resolver, sourceBigAlpha);

        for (const adt::WotlkAdtNormalizationIssue& issue : normalized.issues)
        {
            std::cerr << SeverityName(issue.severity) << " [" << issue.code << "]";
            if (issue.cellSlot != static_cast<std::size_t>(-1))
                std::cerr << " MCNK-slot=" << issue.cellSlot;
            std::cerr << ": " << issue.message << '\n';
        }

        if (!normalized.ready)
        {
            std::cerr << "Conversion blocked: source contains semantics without a verified target mapping.\n";
            return 2;
        }
        if (!normalized.lossless && !allowLossy)
        {
            std::cerr << "Conversion is lossy. Re-run with --allow-lossy only after reviewing every LOSS above.\n";
            return 3;
        }

        const adt::NormalizedAdtBuildResult built = adt::SerializeNormalizedAdt(normalized.adt);
        for (const adt::NormalizedAdtDiagnostic& diagnostic : built.diagnostics)
        {
            std::cerr << "LOSS [LegacyLiquid] MCNK-slot=" << diagnostic.cellSlot
                      << ": " << diagnostic.liquid.message << '\n';
        }
        if (!built.lossless && !allowLossy)
        {
            std::cerr << "Target legacy liquid flattening is lossy; output was not written.\n";
            return 3;
        }

        const adt::AdtValidationReport report = adt::ValidateVanillaAdt(built.adt.bytes);
        WriteAtomicNewFile(outputAdt, built.adt.bytes);

        std::cout << "Converted ADT written: " << outputAdt.string() << '\n'
                  << "MCNK=" << report.mcnkCount
                  << " textures=" << report.textureCount
                  << " M2=" << report.m2PlacementCount
                  << " WMO=" << report.wmoPlacementCount
                  << " liquid-records=" << report.liquidRecordCount << '\n'
                  << "normalization=" << (normalized.lossless && built.lossless ? "lossless" : "lossy-allowed") << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
