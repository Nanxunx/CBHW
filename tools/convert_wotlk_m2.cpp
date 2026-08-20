#include "turtle335/m2/ClassicM2Writer.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> ReadBinary(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open input: " + path.string());

    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0)
        throw std::runtime_error("cannot determine input size: " + path.string());
    in.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    if (!bytes.empty())
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in && !bytes.empty())
        throw std::runtime_error("failed to read input: " + path.string());
    return bytes;
}

void WriteBinary(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
    if (path.has_parent_path())
        fs::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("cannot open output: " + path.string());
    if (!bytes.empty())
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw std::runtime_error("failed to write output: " + path.string());
}

std::string TwoDigits(const std::uint32_t value)
{
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << value;
    return out.str();
}

std::string FourDigits(const std::uint32_t value)
{
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << value;
    return out.str();
}

void PrintUsage(const char* exe)
{
    std::cerr
        << "usage: " << exe
        << " <source.m2> <AnimationData.dbc> <output.m2> [--allow-reference-gated-lights]\n"
        << "\n"
        << "The converter auto-loads WotLK sidecars from the source M2 directory:\n"
        << "  <Stem>00.skin, <Stem>01.skin, ... according to nViews\n"
        << "  <Stem><AnimID:04>-<SubID:02>.anim for external sequences\n";
}

} // namespace

int main(int argc, char** argv)
{
    using namespace turtle335::m2;

    if (argc < 4 || argc > 5)
    {
        PrintUsage(argv[0]);
        return 2;
    }

    try
    {
        const fs::path sourcePath = fs::path(argv[1]);
        const fs::path animationDataPath = fs::path(argv[2]);
        const fs::path outputPath = fs::path(argv[3]);

        ClassicM2WriteOptions options;
        if (argc == 5)
        {
            const std::string option = argv[4];
            if (option != "--allow-reference-gated-lights")
            {
                PrintUsage(argv[0]);
                return 2;
            }
            options.allowReferenceGatedLights = true;
        }

        const auto sourceBytes = ReadBinary(sourcePath);
        const auto animationData = ReadBinary(animationDataPath);
        const auto source = ParseWotlkM2(sourceBytes);

        const fs::path directory = sourcePath.has_parent_path() ? sourcePath.parent_path() : fs::path(".");
        const std::string stem = sourcePath.stem().string();

        std::vector<std::vector<std::uint8_t>> skins;
        skins.reserve(source.viewCount);
        for (std::uint32_t i = 0u; i < source.viewCount; ++i)
        {
            const fs::path skinPath = directory / (stem + TwoDigits(i) + ".skin");
            skins.push_back(ReadBinary(skinPath));
        }

        std::vector<std::vector<std::uint8_t>> externalStorage(source.sequences.size());
        std::vector<const std::vector<std::uint8_t>*> externalBySequence(source.sequences.size(), nullptr);
        std::size_t loadedExternal = 0u;
        std::size_t missingExternal = 0u;

        for (std::size_t i = 0u; i < source.sequences.size(); ++i)
        {
            const auto& sequence = source.sequences[i];
            // Alias sequences borrow payload storage from Sequence.Index; the
            // core resolver follows that chain. Do not report a missing file
            // for an alias-specific name that is not supposed to exist.
            if ((sequence.flags & 0x20u) != 0u || (sequence.flags & 0x40u) != 0u)
                continue;

            const fs::path animPath = directory /
                (stem + FourDigits(sequence.animationId) + "-" + TwoDigits(sequence.subAnimationId) + ".anim");
            if (!fs::exists(animPath))
            {
                ++missingExternal;
                continue;
            }

            externalStorage[i] = ReadBinary(animPath);
            externalBySequence[i] = &externalStorage[i];
            ++loadedExternal;
        }

        const auto result = ConvertWotlkM2ToClassic(
            sourceBytes,
            skins,
            animationData,
            externalBySequence,
            options);
        WriteBinary(outputPath, result.bytes);

        std::cout << "PASS whole-M2 conversion\n"
                  << "source=" << sourcePath.string() << "\n"
                  << "output=" << outputPath.string() << "\n"
                  << "bytes=" << result.bytes.size() << "\n"
                  << "views=" << source.viewCount << "\n"
                  << "external_loaded=" << loadedExternal << "\n"
                  << "external_missing=" << missingExternal << "\n"
                  << "dropped_ribbon_unknown1_nonzero=" << result.droppedRibbonUnknown1NonZero << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL whole-M2 conversion: " << e.what() << "\n";
        return 1;
    }
}
