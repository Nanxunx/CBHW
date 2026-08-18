#include "turtle335/adt/NormalizedAdt.h"

#include <algorithm>
#include <stdexcept>

namespace turtle335::adt {
namespace {

std::size_t Slot(std::uint32_t x, std::uint32_t y)
{
    if (x >= 16 || y >= 16)
        throw std::invalid_argument("normalized ADT MCNK ix/iy must be within 0..15");
    return static_cast<std::size_t>(y) * 16u + static_cast<std::size_t>(x);
}

} // namespace

AdtWriterInput BuildAdtWriterInput(const NormalizedAdt& input,
                                   bool* lossless,
                                   std::vector<NormalizedAdtDiagnostic>* diagnostics)
{
    AdtWriterInput out;
    out.textures = input.textures;
    out.m2Placements = input.m2Placements;
    out.wmoPlacements = input.wmoPlacements;
    out.mfbo = input.targetMfbo;

    bool allLossless = true;
    if (diagnostics)
        diagnostics->clear();

    std::array<bool, 256> occupied{};
    for (const NormalizedAdtCell& source : input.cells)
    {
        const std::size_t slot = Slot(source.ix, source.iy);
        if (occupied[slot])
            throw std::invalid_argument("normalized ADT contains duplicate MCNK ix/iy");
        occupied[slot] = true;

        AdtCellInput& target = out.cells[slot];
        target.header.flags = source.flags;
        target.header.ix = source.ix;
        target.header.iy = source.iy;
        target.header.areaId = source.areaId;
        target.header.holes = source.holes;
        target.header.legacy3E = source.legacy3E;
        target.header.lowQualityTextureMap = source.lowQualityTextureMap;
        target.header.predTex = source.predTex;
        target.header.nEffectDoodad = source.nEffectDoodad;
        target.header.x = source.positionX;
        target.header.z = source.positionZ;
        target.header.props = source.props;
        target.header.effectId = source.effectId;

        const SerializedTerrain terrain = SerializeLegacyTerrain(source.terrain, input.textures.size());
        ApplyTerrainToMcnk(terrain, target.header, target.subchunks);

        const LiquidBuildResult liquid = BuildLegacyMclqBlock(source.liquids);
        target.subchunks.mclq = liquid.block;
        allLossless = allLossless && liquid.lossless;
        if (diagnostics)
        {
            for (const LiquidDiagnostic& diagnostic : liquid.diagnostics)
                diagnostics->push_back({slot, diagnostic});
        }

        target.subchunks.mcsh = source.targetMcsh;
        target.subchunks.mcse = source.targetMcse;
        target.subchunks.mccv = source.targetMccv;
        target.m2Refs = source.m2Refs;
        target.wmoRefs = source.wmoRefs;
    }

    if (std::any_of(occupied.begin(), occupied.end(), [](bool present) { return !present; }))
        throw std::invalid_argument("normalized ADT must contain exactly one MCNK for every ix/iy pair");

    if (lossless)
        *lossless = allLossless;
    return out;
}

NormalizedAdtBuildResult SerializeNormalizedAdt(const NormalizedAdt& input)
{
    NormalizedAdtBuildResult result;
    AdtWriterInput writer = BuildAdtWriterInput(input, &result.lossless, &result.diagnostics);
    result.adt = SerializeVanillaAdt(writer);
    return result;
}

} // namespace turtle335::adt
