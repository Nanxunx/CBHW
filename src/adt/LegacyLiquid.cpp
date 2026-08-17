#include "turtle335/adt/LegacyLiquid.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace turtle335::adt {
namespace {

std::uint32_t PackUv(std::uint16_t u, std::uint16_t v) noexcept
{
    return static_cast<std::uint32_t>(u) | (static_cast<std::uint32_t>(v) << 16);
}

std::pair<std::uint16_t, std::uint16_t> SynthesizeUv(int x, int y) noexcept
{
    return {
        static_cast<std::uint16_t>(std::clamp(x, 0, 8) * 256),
        static_cast<std::uint16_t>(std::clamp(y, 0, 8) * 256)
    };
}

std::uint32_t TargetPayload(const LiquidLayer& layer, const LiquidVertex& vertex, int gx, int gy,
                            bool& reportedMissingUv, LiquidBuildResult& result)
{
    if (layer.category == LiquidCategory::Magma || layer.category == LiquidCategory::Slime)
    {
        std::uint16_t u = 0;
        std::uint16_t v = 0;
        if (vertex.u && vertex.v)
        {
            u = *vertex.u;
            v = *vertex.v;
        }
        else
        {
            const auto uv = SynthesizeUv(gx, gy);
            u = uv.first;
            v = uv.second;
            if (!reportedMissingUv)
            {
                reportedMissingUv = true;
                result.lossless = false;
                result.diagnostics.push_back({
                    LiquidDiagnosticKind::MissingUvSynthesized, gx, gy,
                    "magma/slime source lacks UV data; deterministic legacy UVs synthesized"
                });
            }
        }
        return PackUv(u, v);
    }

    return vertex.depth.value_or(0);
}

void ValidateLayer(const LiquidLayer& layer)
{
    if (!SlotForCategory(layer.category))
        throw std::invalid_argument("cannot encode Unknown liquid category");
    if (layer.width == 0 || layer.height == 0 || layer.offsetX + layer.width > 8 || layer.offsetY + layer.height > 8)
        throw std::invalid_argument("liquid layer rectangle is outside 8x8 MCNK cells");
    if (layer.visible.size() != static_cast<std::size_t>(layer.width) * layer.height)
        throw std::invalid_argument("liquid visibility mask size mismatch");
    if (layer.vertices.size() != static_cast<std::size_t>(layer.width + 1) * (layer.height + 1))
        throw std::invalid_argument("liquid vertex array size mismatch");
}

std::optional<LegacyMclq> BuildCategoryRecord(const std::vector<const LiquidLayer*>& layers,
                                               LiquidCategory category,
                                               float heightEpsilon,
                                               LiquidBuildResult& result)
{
    if (layers.empty())
        return std::nullopt;

    LegacyMclq out;
    out.category = category;
    out.cellFlags.fill(0x0F);
    out.flowData.fill(0);

    std::array<bool, 81> vertexAssigned{};
    std::array<bool, 64> cellAssigned{};
    bool anyVisible = false;
    float minH = std::numeric_limits<float>::infinity();
    float maxH = -std::numeric_limits<float>::infinity();

    for (const LiquidLayer* layerPtr : layers)
    {
        const LiquidLayer& layer = *layerPtr;
        bool reportedMissingUv = false;

        for (int ly = 0; ly < layer.height; ++ly)
        {
            for (int lx = 0; lx < layer.width; ++lx)
            {
                const std::size_t localCell = static_cast<std::size_t>(ly) * layer.width + lx;
                if (!layer.visible[localCell])
                    continue;

                const int tx = layer.offsetX + lx;
                const int ty = layer.offsetY + ly;
                const std::size_t targetCell = static_cast<std::size_t>(ty) * 8 + tx;
                const bool fishable = ((layer.fishableMask >> targetCell) & 1u) != 0;
                const bool fatigueOrDeep = ((layer.deepMask >> targetCell) & 1u) != 0;
                const std::uint8_t desiredFlag = static_cast<std::uint8_t>(
                    LegacyCellCode(category) | (fishable ? 0x40u : 0u) | (fatigueOrDeep ? 0x80u : 0u));

                bool compatibleOverlap = true;
                if (cellAssigned[targetCell])
                {
                    // Same-category duplicate cells are allowed only if their four corner
                    // heights and legacy vertex payloads agree with the existing record.
                    for (int vy = ly; vy <= ly + 1; ++vy)
                    {
                        for (int vx = lx; vx <= lx + 1; ++vx)
                        {
                            const std::size_t srcVertex = static_cast<std::size_t>(vy) * (layer.width + 1) + vx;
                            const int gx = layer.offsetX + vx;
                            const int gy = layer.offsetY + vy;
                            const std::size_t dstVertex = static_cast<std::size_t>(gy) * 9 + gx;
                            const LiquidVertex& sv = layer.vertices[srcVertex];
                            if (!std::isfinite(sv.height))
                                throw std::invalid_argument("non-finite liquid height");

                            const std::uint32_t payload = TargetPayload(layer, sv, gx, gy, reportedMissingUv, result);
                            if (!vertexAssigned[dstVertex] ||
                                std::fabs(out.vertices[dstVertex].height - sv.height) > heightEpsilon ||
                                out.vertices[dstVertex].lightOrUv != payload)
                            {
                                compatibleOverlap = false;
                            }
                        }
                    }

                    if (!compatibleOverlap)
                    {
                        result.lossless = false;
                        result.diagnostics.push_back({
                            LiquidDiagnosticKind::OverlappingCells, tx, ty,
                            "same-category MH2O layers overlap with incompatible legacy surface data"
                        });
                        continue; // first compatible surface wins
                    }

                    // Same surface: combine gameplay attributes conservatively.
                    out.cellFlags[targetCell] |= static_cast<std::uint8_t>(desiredFlag & 0xC0u);
                    continue;
                }

                cellAssigned[targetCell] = true;
                out.cellFlags[targetCell] = desiredFlag;
                anyVisible = true;

                for (int vy = ly; vy <= ly + 1; ++vy)
                {
                    for (int vx = lx; vx <= lx + 1; ++vx)
                    {
                        const std::size_t srcVertex = static_cast<std::size_t>(vy) * (layer.width + 1) + vx;
                        const int gx = layer.offsetX + vx;
                        const int gy = layer.offsetY + vy;
                        const std::size_t dstVertex = static_cast<std::size_t>(gy) * 9 + gx;
                        const LiquidVertex& sv = layer.vertices[srcVertex];

                        if (!std::isfinite(sv.height))
                            throw std::invalid_argument("non-finite liquid height");

                        const std::uint32_t payload = TargetPayload(layer, sv, gx, gy, reportedMissingUv, result);

                        if (vertexAssigned[dstVertex])
                        {
                            if (std::fabs(out.vertices[dstVertex].height - sv.height) > heightEpsilon)
                            {
                                result.lossless = false;
                                result.diagnostics.push_back({
                                    LiquidDiagnosticKind::SharedVertexHeightConflict, gx, gy,
                                    "same-category MH2O instances require different heights for one legacy vertex"
                                });
                                continue;
                            }
                            if (out.vertices[dstVertex].lightOrUv != payload)
                            {
                                result.lossless = false;
                                result.diagnostics.push_back({
                                    LiquidDiagnosticKind::SharedVertexPayloadConflict, gx, gy,
                                    "same-category MH2O instances require different legacy payloads for one vertex"
                                });
                                continue;
                            }
                        }

                        vertexAssigned[dstVertex] = true;
                        out.vertices[dstVertex].height = sv.height;
                        out.vertices[dstVertex].lightOrUv = payload;
                        minH = std::min(minH, sv.height);
                        maxH = std::max(maxH, sv.height);
                    }
                }
            }
        }
    }

    if (!anyVisible)
        return std::nullopt;

    const float fillH = std::isfinite(minH) ? minH : 0.0f;
    for (std::size_t i = 0; i < out.vertices.size(); ++i)
    {
        if (!vertexAssigned[i])
            out.vertices[i].height = fillH;
    }

    out.minHeight = std::isfinite(minH) ? minH : fillH;
    out.maxHeight = std::isfinite(maxH) ? maxH : fillH;
    return out;
}

} // namespace

std::size_t LegacyMclqRecordCount(const LegacyMclqBlock& block) noexcept
{
    std::size_t count = 0;
    for (const auto& record : block.records)
        count += record.has_value() ? 1u : 0u;
    return count;
}

LiquidBuildResult BuildLegacyMclqBlock(const std::vector<LiquidLayer>& layers, float heightEpsilon)
{
    LiquidBuildResult result;
    std::array<std::vector<const LiquidLayer*>, 4> grouped;

    for (const LiquidLayer& layer : layers)
    {
        ValidateLayer(layer);
        const auto slot = SlotForCategory(layer.category);
        grouped[SlotIndex(*slot)].push_back(&layer);
    }

    constexpr std::array<LiquidCategory, 4> categories = {
        LiquidCategory::Water,
        LiquidCategory::Ocean,
        LiquidCategory::Magma,
        LiquidCategory::Slime
    };

    for (std::size_t i = 0; i < categories.size(); ++i)
    {
        result.block.records[i] = BuildCategoryRecord(grouped[i], categories[i], heightEpsilon, result);
        if (result.block.records[i])
            result.block.mcnkLiquidFlags |= McnkLiquidFlag(categories[i]);
    }

    return result;
}

} // namespace turtle335::adt
