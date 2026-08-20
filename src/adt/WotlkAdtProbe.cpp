#include "turtle335/adt/WotlkAdtProbe.h"

#include "turtle335/adt/Mcal.h"
#include "turtle335/adt/Mh2oReader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kMclyUseAlpha = 0x100u;
constexpr std::uint32_t kMclyAlphaCompressed = 0x200u;
constexpr std::uint32_t kHighResolutionHoles = 1u << 16;
constexpr std::uint32_t kKnownSourceFlags =
    0x01u | 0x02u | 0x3Cu | 0x40u | (1u << 15) | kHighResolutionHoles;

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset,
                      const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside source chunk bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::size_t RawPayloadSize(const std::vector<std::uint8_t>& chunk,
                           const char rawId[4],
                           const char* what)
{
    if (chunk.empty())
        return 0;
    if (chunk.size() < 8)
        throw std::runtime_error(std::string(what) + " is shorter than a chunk header");
    if (std::memcmp(chunk.data(), rawId, 4) != 0)
        throw std::runtime_error(std::string(what) + " raw FourCC mismatch");
    const std::size_t payload = ReadU32(chunk, 4, what);
    if (payload + 8u != chunk.size())
        throw std::runtime_error(std::string(what) + " declared size disagrees with bytes");
    return payload;
}

bool HasLegacyMclqBlock(const std::vector<std::uint8_t>& chunk)
{
    if (chunk.empty())
        return false;
    if (chunk.size() < 8u)
        throw std::runtime_error("MCLQ is shorter than a chunk header");
    if (std::memcmp(chunk.data(), "QLCM", 4) != 0)
        throw std::runtime_error("MCLQ raw FourCC mismatch");

    const std::uint32_t innerPayload = ReadU32(chunk, 4u, "MCLQ inner size");
    // Legacy MCLQ may deliberately keep the inner size at zero while the
    // owning MCNK.sizeMCLQ describes the complete 8+804*N block copied by the
    // source reader. A nonzero inner size must still agree with the bytes.
    if (innerPayload != 0 && static_cast<std::size_t>(innerPayload) + 8u != chunk.size())
        throw std::runtime_error("MCLQ inner size disagrees with MCNK-owned block bytes");
    return chunk.size() > 8u || innerPayload != 0;
}

bool AnyNonZero(const std::array<std::uint8_t, 8>& bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](std::uint8_t value) { return value != 0; });
}

void AddIssue(WotlkAdtProbeReport& report,
              bool blocker,
              std::size_t slot,
              std::string code,
              std::string message)
{
    report.issues.push_back({blocker, slot, std::move(code), std::move(message)});
}

WotlkAlphaEncoding InspectAlpha(const WotlkMcnkRecord& cell,
                                std::optional<bool> sourceWdtBigAlpha,
                                WotlkAdtProbeReport& report,
                                std::size_t slot)
{
    if (cell.header.nLayers == 0 || cell.header.nLayers > 4)
    {
        AddIssue(report, true, slot, "InvalidLayerCount",
                 "MCNK texture layer count is outside the build12340 1..4 range");
        return WotlkAlphaEncoding::Unknown;
    }

    try
    {
        const std::size_t mclyPayload = RawPayloadSize(cell.mcly, "YLCM", "MCLY");
        if (mclyPayload != static_cast<std::size_t>(cell.header.nLayers) * 16u)
        {
            AddIssue(report, true, slot, "InvalidMclySize",
                     "MCLY payload does not equal nLayers*16");
            return WotlkAlphaEncoding::Unknown;
        }

        struct LayerAlpha
        {
            bool usesAlpha = false;
            bool compressed = false;
            std::uint32_t offset = 0;
        };

        std::vector<LayerAlpha> layers(cell.header.nLayers);
        bool anyAlpha = false;
        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            const std::size_t at = 8u + i * 16u;
            const std::uint32_t flags = ReadU32(cell.mcly, at + 4u, "MCLY flags");
            layers[i].usesAlpha = (flags & kMclyUseAlpha) != 0;
            layers[i].compressed = (flags & kMclyAlphaCompressed) != 0;
            layers[i].offset = ReadU32(cell.mcly, at + 8u, "MCLY alpha offset");
            if (i == 0 && layers[i].usesAlpha)
            {
                AddIssue(report, true, slot, "BaseLayerUsesAlpha",
                         "base MCLY layer unexpectedly requests an alpha map");
                return WotlkAlphaEncoding::Unknown;
            }
            if (i > 0 && layers[i].usesAlpha)
                anyAlpha = true;
        }

        if (!anyAlpha)
            return WotlkAlphaEncoding::None;

        const std::size_t mcalPayload = RawPayloadSize(cell.mcal, "LACM", "MCAL");
        if (mcalPayload == 0)
        {
            AddIssue(report, true, slot, "MissingMcal",
                     "MCLY requests alpha maps but MCAL has no payload");
            return WotlkAlphaEncoding::Unknown;
        }
        const std::uint8_t* payload = cell.mcal.data() + 8u;

        bool sawLegacy = false;
        bool sawBig = false;
        bool sawRle = false;
        bool sawUnknown = false;

        for (std::size_t layerIndex = 1; layerIndex < layers.size(); ++layerIndex)
        {
            const LayerAlpha& layer = layers[layerIndex];
            if (!layer.usesAlpha)
                continue;
            if (layer.offset >= mcalPayload)
            {
                sawUnknown = true;
                AddIssue(report, true, slot, "AlphaOffsetOutOfRange",
                         "MCLY alpha offset is outside MCAL payload");
                continue;
            }

            std::size_t end = mcalPayload;
            for (std::size_t later = layerIndex + 1; later < layers.size(); ++later)
            {
                const std::size_t candidate = layers[later].offset;
                if (candidate > layer.offset && candidate < end)
                    end = candidate;
            }
            if (end <= layer.offset)
            {
                sawUnknown = true;
                AddIssue(report, true, slot, "EmptyAlphaSlice",
                         "MCLY alpha offsets produce an empty or reversed MCAL slice");
                continue;
            }

            const std::size_t sliceSize = end - layer.offset;
            const std::uint8_t* slice = payload + layer.offset;
            if (layer.compressed)
            {
                try
                {
                    (void)DecodeRleAlpha8(slice, sliceSize);
                    sawRle = true;
                }
                catch (const std::exception& error)
                {
                    sawUnknown = true;
                    AddIssue(report, true, slot, "MalformedRleAlpha", error.what());
                }
            }
            else if (sliceSize == 2048u)
            {
                sawLegacy = true;
            }
            else if (sliceSize == 4096u)
            {
                sawBig = true;
            }
            else
            {
                sawUnknown = true;
                AddIssue(report, true, slot, "UnknownAlphaSliceSize",
                         "uncompressed MCAL slice is neither 2048-byte legacy4 nor 4096-byte big8");
            }
        }

        const int kinds = (sawLegacy ? 1 : 0) + (sawBig ? 1 : 0) + (sawRle ? 1 : 0);
        WotlkAlphaEncoding encoding = WotlkAlphaEncoding::None;
        if (sawUnknown)
            encoding = WotlkAlphaEncoding::Unknown;
        else if (kinds > 1)
            encoding = WotlkAlphaEncoding::Mixed;
        else if (sawLegacy)
            encoding = WotlkAlphaEncoding::Legacy4;
        else if (sawBig)
            encoding = WotlkAlphaEncoding::Big8;
        else if (sawRle)
            encoding = WotlkAlphaEncoding::Rle8;

        if (encoding == WotlkAlphaEncoding::Mixed)
        {
            AddIssue(report, true, slot, "MixedAlphaStorage",
                     "one MCNK mixes legacy4/big8/RLE alpha storage; production normalizer currently rejects this");
        }

        if (sourceWdtBigAlpha.has_value())
        {
            if (*sourceWdtBigAlpha && encoding == WotlkAlphaEncoding::Legacy4)
            {
                AddIssue(report, false, slot, "WdtBigAlphaMismatch",
                         "WDT advertises big alpha but this MCNK uses 2048-byte legacy alpha slices");
            }
            else if (!*sourceWdtBigAlpha &&
                     (encoding == WotlkAlphaEncoding::Big8 || encoding == WotlkAlphaEncoding::Rle8))
            {
                AddIssue(report, false, slot, "WdtLegacyAlphaMismatch",
                         "WDT does not advertise big alpha but this MCNK uses big/RLE alpha storage");
            }
        }
        return encoding;
    }
    catch (const std::exception& error)
    {
        AddIssue(report, true, slot, "AlphaProbeFailure", error.what());
        return WotlkAlphaEncoding::Unknown;
    }
}

void CountAlpha(WotlkAdtProbeReport& report, WotlkAlphaEncoding encoding)
{
    switch (encoding)
    {
        case WotlkAlphaEncoding::None: ++report.cellsNoAlpha; break;
        case WotlkAlphaEncoding::Legacy4: ++report.cellsLegacy4Alpha; break;
        case WotlkAlphaEncoding::Big8: ++report.cellsBig8Alpha; break;
        case WotlkAlphaEncoding::Rle8: ++report.cellsRle8Alpha; break;
        case WotlkAlphaEncoding::Mixed: ++report.cellsMixedAlpha; break;
        case WotlkAlphaEncoding::Unknown: ++report.cellsUnknownAlpha; break;
    }
}

} // namespace

WotlkAdtProbeReport ProbeWotlkAdt(const WotlkAdtDocument& source,
                                  std::optional<bool> sourceWdtBigAlpha)
{
    WotlkAdtProbeReport report;
    report.version = source.version;
    report.textureCount = source.textures.size();
    report.m2PlacementCount = source.m2Placements.size();
    report.wmoPlacementCount = source.wmoPlacements.size();
    report.hasMh2o = !source.mh2o.empty();
    report.hasMfbo = !source.mfbo.empty();

    if (source.version != 18)
        AddIssue(report, true, static_cast<std::size_t>(-1), "UnexpectedMver", "source ADT MVER is not 18");

    for (std::size_t slot = 0; slot < source.cells.size(); ++slot)
    {
        const WotlkMcnkRecord& cell = source.cells[slot];
        CountAlpha(report, InspectAlpha(cell, sourceWdtBigAlpha, report, slot));

        if ((cell.header.flags & kHighResolutionHoles) != 0)
        {
            ++report.cellsHighResolutionHoles;
            AddIssue(report, true, slot, "HighResolutionHoles",
                     "source MCNK uses high-resolution holes, currently a target-conversion blocker");
        }
        if (cell.header.legacy3E != 0)
            ++report.cellsNonzeroField3E;
        if (AnyNonZero(cell.header.disableDoodadsMap))
            ++report.cellsDisableDoodadsMap;
        if (cell.header.unused1 != 0 || cell.header.unused2 != 0)
            ++report.cellsNonzeroTailDwords;
        if ((cell.header.flags & ~kKnownSourceFlags) != 0)
            ++report.cellsUnverifiedFlags;

        for (std::uint32_t ref : cell.m2Refs)
        {
            if (ref >= source.m2Placements.size())
            {
                ++report.invalidM2References;
                AddIssue(report, true, slot, "InvalidM2Reference",
                         "MCRF M2 reference exceeds MDDF placement table");
            }
        }
        for (std::uint32_t ref : cell.wmoRefs)
        {
            if (ref >= source.wmoPlacements.size())
            {
                ++report.invalidWmoReferences;
                AddIssue(report, true, slot, "InvalidWmoReference",
                         "MCRF WMO reference exceeds MODF placement table");
            }
        }

        try
        {
            const std::size_t mcshPayload = RawPayloadSize(cell.mcsh, "HSCM", "MCSH");
            if (mcshPayload != 0)
            {
                ++report.cellsWithMcsh;
                if (mcshPayload != 512u)
                    AddIssue(report, true, slot, "InvalidMcshSize", "MCSH payload is not 512 bytes");
            }

            const std::size_t mccvPayload = RawPayloadSize(cell.mccv, "VCCM", "MCCV");
            if (mccvPayload != 0)
            {
                ++report.cellsWithMccv;
                if (mccvPayload != 145u * 4u)
                    AddIssue(report, false, slot, "UnexpectedMccvSize", "MCCV payload is not 145*4 bytes");
                AddIssue(report, false, slot, "TargetMccvLoss",
                         "Turtle production target does not currently preserve source MCCV");
            }

            const std::size_t mcsePayload = RawPayloadSize(cell.mcse, "ESCM", "MCSE");
            if (mcsePayload != 0)
            {
                ++report.cellsWithMcse;
                if ((mcsePayload % 28u) != 0)
                {
                    AddIssue(report, true, slot, "InvalidMcseSize",
                             "MCSE payload is not divisible by the 28-byte build12340 emitter size");
                }
                else
                {
                    const std::size_t emitters = mcsePayload / 28u;
                    report.soundEmitterCount += emitters;
                    if (emitters != cell.header.nSndEmitters)
                    {
                        AddIssue(report, true, slot, "McseCountMismatch",
                                 "MCSE payload emitter count disagrees with MCNK.nSndEmitters");
                    }
                }
                AddIssue(report, false, slot, "TargetMcseUnverified",
                         "source MCSE is present; target emitter field semantics are not yet production-approved");
            }

            if (HasLegacyMclqBlock(cell.mclq) || (cell.header.flags & 0x3Cu) != 0)
                ++report.cellsWithLegacyMclq;
        }
        catch (const std::exception& error)
        {
            AddIssue(report, true, slot, "SubchunkProbeFailure", error.what());
        }
    }

    if (report.cellsNonzeroField3E != 0)
        AddIssue(report, false, static_cast<std::size_t>(-1), "SourceField3EUsed",
                 "one or more MCNKs use source +0x3E; target canonical writer currently drops it");
    if (report.cellsDisableDoodadsMap != 0)
        AddIssue(report, false, static_cast<std::size_t>(-1), "DisableDoodadsMapUsed",
                 "one or more MCNKs use WotLK disable_doodads_map bytes; target mapping is not yet implemented");
    if (report.cellsNonzeroTailDwords != 0)
        AddIssue(report, false, static_cast<std::size_t>(-1), "SourceTailDwordsUsed",
                 "one or more MCNKs use source unused1/unused2; they are not alias-copied into legacy target fields");
    if (report.cellsUnverifiedFlags != 0)
        AddIssue(report, false, static_cast<std::size_t>(-1), "UnverifiedMcnkFlagsUsed",
                 "one or more MCNKs contain source flag bits without a verified target semantic mapping");
    if (report.hasMfbo)
        AddIssue(report, false, static_cast<std::size_t>(-1), "TargetMfboLoss",
                 "source MFBO is present; target semantic normalization is not implemented yet");

    if (report.hasMh2o)
    {
        try
        {
            const Mh2oParseResult parsed = ParseMh2oChunk(
                source.mh2o.data(), source.mh2o.size(),
                [](std::uint16_t) { return LiquidCategory::Unknown; });

            std::map<std::uint16_t, std::size_t> layerCounts;
            std::map<std::uint16_t, std::set<std::size_t>> cellSets;
            for (std::size_t slot = 0; slot < parsed.chunks.size(); ++slot)
            {
                for (const ParsedMh2oLayer& layer : parsed.chunks[slot])
                {
                    ++layerCounts[layer.metadata.sourceLiquidType];
                    cellSets[layer.metadata.sourceLiquidType].insert(slot);
                }
            }
            for (const auto& entry : layerCounts)
            {
                WotlkLiquidTypeUsage usage;
                usage.sourceLiquidType = entry.first;
                usage.layerCount = entry.second;
                usage.cellCount = cellSets[entry.first].size();
                report.liquidTypes.push_back(usage);
            }
        }
        catch (const std::exception& error)
        {
            report.mh2oParsed = false;
            AddIssue(report, true, static_cast<std::size_t>(-1), "MalformedMh2o", error.what());
        }
    }

    return report;
}

} // namespace turtle335::adt
