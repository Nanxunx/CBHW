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
    // Turtle scales legacy u16 coordinates by 3/256. Choosing 256 per grid step
    // yields a deterministic 3.0 texture-coordinate step. This is intentionally
    // marked diagnostic until fixture-validated against known-good Vanilla magma.
    return {
        static_cast<std::uint16_t>(std::clamp(x, 0, 8) * 256),
        static_cast<std::uint16_t>(std::clamp(y, 0, 8) * 256)
    };
}

} // namespace

LiquidBuildResult BuildLegacyMclq(const std::vector<LiquidLayer>& layers, float heightEpsilon)
{
    LiquidBuildResult result;
    if (layers.empty())
        return result;

    LegacyMclq out;
    out.cellFlags.fill(0x0F);
    out.flowData.fill(0);

    std::array<bool, 81> heightAssigned{};
    std::array<std::optional<LiquidCategory>, 64> cellOwner{};
    std::array<std::optional<LiquidCategory>, 81> vertexOwner{};

    bool hasMagma = false;
    bool hasSlime = false;
    bool anyVisible = false;

    float minH = std::numeric_limits<float>::infinity();
    float maxH = -std::numeric_limits<float>::infinity();

    for (const LiquidLayer& layer : layers)
    {
        if (layer.category == LiquidCategory::Unknown)
            throw std::invalid_argument("cannot encode Unknown liquid category");
        if (layer.width == 0 || layer.height == 0 || layer.offsetX + layer.width > 8 || layer.offsetY + layer.height > 8)
            throw std::invalid_argument("liquid layer rectangle is outside 8x8 MCNK cells");
        if (layer.visible.size() != static_cast<std::size_t>(layer.width) * layer.height)
            throw std::invalid_argument("liquid visibility mask size mismatch");
        if (layer.vertices.size() != static_cast<std::size_t>(layer.width + 1) * (layer.height + 1))
            throw std::invalid_argument("liquid vertex array size mismatch");

        hasMagma = hasMagma || layer.category == LiquidCategory::Magma;
        hasSlime = hasSlime || layer.category == LiquidCategory::Slime;
        out.mcnkLiquidFlags |= McnkLiquidFlag(layer.category);

        bool reportedMissingUv = false;
        bool reportedFishable = false;

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

                if (cellOwner[targetCell].has_value())
                {
                    result.lossless = false;
                    result.diagnostics.push_back({
                        LiquidDiagnosticKind::OverlappingCells, tx, ty,
                        "multiple MH2O layers claim the same legacy MCLQ cell"
                    });
                    // First visible layer wins in the baseline policy.
                    continue;
                }

                cellOwner[targetCell] = layer.category;
                std::uint8_t flag = LegacyCellCode(layer.category);
                if (layer.category == LiquidCategory::Ocean && ((layer.deepMask >> targetCell) & 1u))
                    flag |= 0x80;
                out.cellFlags[targetCell] = flag;
                anyVisible = true;

                if (!reportedFishable && ((layer.fishableMask >> targetCell) & 1u))
                {
                    reportedFishable = true;
                    result.diagnostics.push_back({
                        LiquidDiagnosticKind::FishableNotEncoded, tx, ty,
                        "MH2O fishable attribute retained only as diagnostic; no target MCLQ bit is invented"
                    });
                }

                // Assign the four vertices needed by this visible cell.
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

                        if (heightAssigned[dstVertex] && std::fabs(out.vertices[dstVertex].height - sv.height) > heightEpsilon)
                        {
                            result.lossless = false;
                            result.diagnostics.push_back({
                                LiquidDiagnosticKind::SharedVertexHeightConflict, gx, gy,
                                "adjacent MH2O layers require different heights for one legacy MCLQ vertex"
                            });
                            continue; // preserve first assignment
                        }

                        const bool sourceUsesUv = layer.category == LiquidCategory::Magma || layer.category == LiquidCategory::Slime;
                        if (vertexOwner[dstVertex].has_value())
                        {
                            const LiquidCategory oldCategory = *vertexOwner[dstVertex];
                            const bool oldUsesUv = oldCategory == LiquidCategory::Magma || oldCategory == LiquidCategory::Slime;
                            if (oldUsesUv != sourceUsesUv)
                            {
                                result.lossless = false;
                                result.diagnostics.push_back({
                                    LiquidDiagnosticKind::SharedVertexPayloadConflict, gx, gy,
                                    "one legacy MCLQ vertex cannot simultaneously encode water depth and magma/slime UV payloads"
                                });
                                continue; // preserve first representation
                            }
                        }

                        heightAssigned[dstVertex] = true;
                        vertexOwner[dstVertex] = layer.category;
                        out.vertices[dstVertex].height = sv.height;
                        minH = std::min(minH, sv.height);
                        maxH = std::max(maxH, sv.height);

                        if (layer.category == LiquidCategory::Magma || layer.category == LiquidCategory::Slime)
                        {
                            std::uint16_t u = 0;
                            std::uint16_t v = 0;
                            if (sv.u && sv.v)
                            {
                                u = *sv.u;
                                v = *sv.v;
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
                            out.vertices[dstVertex].lightOrUv = PackUv(u, v);
                        }
                        else
                        {
                            out.vertices[dstVertex].lightOrUv = sv.depth.value_or(0);
                        }
                    }
                }
            }
        }
    }

    if (hasMagma && hasSlime)
    {
        result.lossless = false;
        result.diagnostics.push_back({
            LiquidDiagnosticKind::MagmaSlimeMixed, -1, -1,
            "legacy cell selector 0x06 cannot independently encode magma vs slime semantics inside one MCNK"
        });
    }

    if (!anyVisible)
        return result;

    // Hidden/unassigned vertices are irrelevant to rendering. Fill them with a stable
    // valid height to prevent NaNs and pathological culling ranges.
    const float fillH = std::isfinite(minH) ? minH : 0.0f;
    for (std::size_t i = 0; i < out.vertices.size(); ++i)
    {
        if (!heightAssigned[i])
            out.vertices[i].height = fillH;
    }

    out.minHeight = std::isfinite(minH) ? minH : fillH;
    out.maxHeight = std::isfinite(maxH) ? maxH : fillH;
    result.mclq = out;
    return result;
}

} // namespace turtle335::adt
