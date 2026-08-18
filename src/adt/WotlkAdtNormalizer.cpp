#include "turtle335/adt/WotlkAdtNormalizer.h"

#include "turtle335/adt/WotlkMcshNormalizer.h"
#include "turtle335/adt/WotlkTerrainNormalizer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kSourceHighResHoles = 1u << 16;
constexpr std::uint32_t kTargetImpassible = 0x02u;
constexpr std::uint32_t kWriterOwnedSourceFlags = 0x01u | 0x3Cu | 0x40u | (1u << 15);

std::uint32_t RawPayloadSize(const std::vector<std::uint8_t>& chunk, const char* what)
{
    if (chunk.empty())
        return 0;
    if (chunk.size() < 8)
        throw std::runtime_error(std::string(what) + " raw chunk is truncated");
    const std::uint32_t size = static_cast<std::uint32_t>(chunk[4]) |
                               (static_cast<std::uint32_t>(chunk[5]) << 8) |
                               (static_cast<std::uint32_t>(chunk[6]) << 16) |
                               (static_cast<std::uint32_t>(chunk[7]) << 24);
    if (static_cast<std::size_t>(size) + 8u != chunk.size())
        throw std::runtime_error(std::string(what) + " declared size disagrees with bytes");
    return size;
}

bool AnyNonZero(const std::array<std::uint8_t, 8>& bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](std::uint8_t value) { return value != 0; });
}

void AddIssue(WotlkAdtNormalizationResult& result,
              WotlkAdtIssueSeverity severity,
              std::size_t slot,
              std::string code,
              std::string message)
{
    result.issues.push_back({severity, slot, std::move(code), std::move(message)});
    result.lossless = false;
    if (severity == WotlkAdtIssueSeverity::Blocker)
        result.ready = false;
}

} // namespace

WotlkAdtNormalizationResult NormalizeWotlkAdt(const WotlkAdtDocument& source,
                                               const LiquidTypeResolver& liquidTypeResolver,
                                               bool sourceWdtBigAlpha)
{
    if (source.version != 18)
        throw std::invalid_argument("WotLK ADT normalizer requires source MVER 18");
    if (source.textures.empty())
        throw std::invalid_argument("WotLK ADT normalizer requires a non-empty MTEX catalog");

    WotlkAdtNormalizationResult result;
    result.adt.textures = source.textures;
    result.adt.m2Placements = source.m2Placements;
    result.adt.wmoPlacements = source.wmoPlacements;

    if (RawPayloadSize(source.mfbo, "MFBO") != 0)
    {
        AddIssue(result, WotlkAdtIssueSeverity::Loss, static_cast<std::size_t>(-1),
                 "DroppedMfbo",
                 "source MFBO is not raw-copied because target fallback-bounds semantics are not yet normalized");
    }

    Mh2oParseResult mh2o;
    bool haveMh2o = !source.mh2o.empty();
    if (haveMh2o)
    {
        if (!liquidTypeResolver)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Blocker, static_cast<std::size_t>(-1),
                     "MissingLiquidTypeResolver",
                     "source has MH2O but no LiquidType.dbc category resolver was supplied");
            haveMh2o = false;
        }
        else
        {
            mh2o = ParseMh2oChunk(source.mh2o.data(), source.mh2o.size(), liquidTypeResolver);
        }
    }

    for (std::size_t slot = 0; slot < source.cells.size(); ++slot)
    {
        const WotlkMcnkRecord& src = source.cells[slot];
        NormalizedAdtCell& dst = result.adt.cells[slot];

        dst.ix = src.header.ix;
        dst.iy = src.header.iy;
        dst.flags = src.header.flags & kTargetImpassible;
        dst.areaId = src.header.areaId;
        dst.holes = src.header.holes;
        dst.lowQualityTextureMap = src.header.lowQualityTextureMap;
        dst.positionX = src.header.x;
        dst.positionZ = src.header.z;
        dst.m2Refs = src.m2Refs;
        dst.wmoRefs = src.wmoRefs;

        for (std::uint32_t ref : dst.m2Refs)
        {
            if (ref >= source.m2Placements.size())
                AddIssue(result, WotlkAdtIssueSeverity::Blocker, slot, "InvalidM2Reference",
                         "MCRF M2 reference exceeds MDDF placement table");
        }
        for (std::uint32_t ref : dst.wmoRefs)
        {
            if (ref >= source.wmoPlacements.size())
                AddIssue(result, WotlkAdtIssueSeverity::Blocker, slot, "InvalidWmoReference",
                         "MCRF WMO reference exceeds MODF placement table");
        }

        if ((src.header.flags & kSourceHighResHoles) != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Blocker, slot, "HighResolutionHoles",
                     "source MCNK requests high-resolution holes; the verified Turtle target path currently emits the 16-bit coarse mask only");
        }

        const std::uint32_t unverifiedFlags =
            src.header.flags & ~(kWriterOwnedSourceFlags | kTargetImpassible | kSourceHighResHoles);
        if (unverifiedFlags != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedUnverifiedMcnkFlags",
                     "source MCNK contains flag bits without a verified Vanilla/Turtle semantic mapping");
        }

        if (src.header.legacy3E != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedSourceField3E",
                     "source MCNK +0x3E is not assumed equivalent to the legacy target padding field");
        }
        if (AnyNonZero(src.header.disableDoodadsMap))
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedDisableDoodadsMap",
                     "WotLK disable-doodads/high-detail map bytes are not alias-copied into legacy predTex/nEffectDoodad fields");
        }
        if (src.header.unused1 != 0 || src.header.unused2 != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedUnverifiedMcnkTail",
                     "WotLK final MCNK dwords are not alias-copied into legacy props/effectId fields");
        }

        if (RawPayloadSize(src.mcsh, "MCSH") != 0)
        {
            dst.targetMcsh = NormalizeWotlkMcsh(src.mcsh, (src.header.flags & (1u << 15)) != 0);
        }
        if (RawPayloadSize(src.mccv, "MCCV") != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedMccv",
                     "source terrain vertex colors are not yet normalized into target MCCV");
        }
        if (RawPayloadSize(src.mcse, "MCSE") != 0)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "DroppedMcse",
                     "source terrain sound emitters are not yet normalized into target MCSE");
        }

        const WotlkTerrainNormalizationResult terrain =
            NormalizeWotlkTerrain(src, source.textures.size(), sourceWdtBigAlpha);
        dst.terrain = terrain.terrain;

        const bool sourceLegacyLiquid = (src.header.flags & 0x3Cu) != 0 || RawPayloadSize(src.mclq, "MCLQ") != 0;
        if (haveMh2o)
        {
            if (sourceLegacyLiquid)
            {
                AddIssue(result, WotlkAdtIssueSeverity::Loss, slot, "IgnoredLegacyMclqAlongsideMh2o",
                         "MH2O is authoritative; legacy source MCLQ/category bits are not merged blindly into it");
            }
            for (const ParsedMh2oLayer& layer : mh2o.chunks[slot])
            {
                if (layer.layer.category == LiquidCategory::Unknown)
                {
                    AddIssue(result, WotlkAdtIssueSeverity::Blocker, slot, "UnknownLiquidType",
                             "LiquidType.dbc resolver returned Unknown for an MH2O source layer");
                }
                dst.liquids.push_back(layer.layer);
            }
        }
        else if (sourceLegacyLiquid)
        {
            AddIssue(result, WotlkAdtIssueSeverity::Blocker, slot, "LegacySourceMclqNeedsParser",
                     "source liquid exists without usable MH2O; semantic legacy-MCLQ import must be explicit rather than raw-copied");
        }
    }

    return result;
}

} // namespace turtle335::adt
