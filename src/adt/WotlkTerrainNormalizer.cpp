#include "turtle335/adt/WotlkTerrainNormalizer.h"

#include "turtle335/adt/Mcal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kMclyUseAlpha = 0x100u;
constexpr std::uint32_t kMclyAlphaCompressed = 0x200u;
constexpr std::uint32_t kTargetSemanticMclyMask = 0x4FFu; // animation/glow/reflection, excludes writer-owned alpha bits
constexpr std::uint32_t kDoNotFixAlphaMap = 1u << 15;

struct RawLayer
{
    std::uint32_t textureId = 0;
    std::uint32_t flags = 0;
    std::uint32_t alphaOffset = 0;
    std::uint32_t effectId = 0xFFFFu;
};

void RequireRawChunk(const std::vector<std::uint8_t>& chunk,
                     const char rawId[4],
                     std::size_t expectedPayload,
                     const char* what)
{
    if (chunk.size() != expectedPayload + 8u)
        throw std::runtime_error(std::string(what) + " size is not canonical for build 12340 terrain");
    if (std::memcmp(chunk.data(), rawId, 4) != 0)
        throw std::runtime_error(std::string(what) + " raw FourCC mismatch");
    const std::uint32_t declared = static_cast<std::uint32_t>(chunk[4]) |
                                   (static_cast<std::uint32_t>(chunk[5]) << 8) |
                                   (static_cast<std::uint32_t>(chunk[6]) << 16) |
                                   (static_cast<std::uint32_t>(chunk[7]) << 24);
    if (declared != expectedPayload)
        throw std::runtime_error(std::string(what) + " declared payload size mismatch");
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside chunk bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

float ReadF32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    const std::uint32_t bits = ReadU32(bytes, offset, what);
    float value = 0.0f;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value))
        throw std::runtime_error(std::string(what) + " is non-finite");
    return value;
}

void ApplyLegacyEdgeFix(Alpha8& alpha)
{
    for (std::size_t i = 0; i < 64; ++i)
    {
        alpha[i * 64u + 63u] = alpha[i * 64u + 62u];
        alpha[63u * 64u + i] = alpha[62u * 64u + i];
    }
    alpha[63u * 64u + 63u] = alpha[62u * 64u + 62u];
}

void ConvertSequentialToIndependent(std::vector<Alpha8*>& maps)
{
    if (maps.empty())
        return;

    for (std::size_t pixel = 0; pixel < 64u * 64u; ++pixel)
    {
        int remaining = 255;
        for (std::size_t reverse = maps.size(); reverse-- > 0; )
        {
            const int source = (*maps[reverse])[pixel];
            const int product = source * remaining;
            const int converted = product / 255 + ((product % 255) > 127 ? 1 : 0);
            (*maps[reverse])[pixel] = static_cast<std::uint8_t>(converted);
            remaining -= converted;
        }
    }
}

void ValidateIndependentAlphaTotals(const TerrainCellInput& terrain)
{
    if (terrain.layers.size() <= 1)
        return;

    for (std::size_t pixel = 0; pixel < 64u * 64u; ++pixel)
    {
        unsigned total = 0;
        for (std::size_t layer = 1; layer < terrain.layers.size(); ++layer)
        {
            if (!terrain.layers[layer].alpha)
                throw std::logic_error("normalized non-base terrain layer unexpectedly lacks alpha");
            total += (*terrain.layers[layer].alpha)[pixel];
        }
        if (total > 255u)
            throw std::runtime_error("WotLK independent alpha contributions exceed 255 at one pixel");
    }
}

} // namespace

WotlkTerrainNormalizationResult NormalizeWotlkTerrain(const WotlkMcnkRecord& cell,
                                                       std::size_t sourceTextureCount,
                                                       bool sourceWdtBigAlpha)
{
    if (cell.header.nLayers == 0 || cell.header.nLayers > 4)
        throw std::runtime_error("WotLK MCNK terrain layer count must be 1..4 for Vanilla retroport");
    if (sourceTextureCount == 0)
        throw std::runtime_error("WotLK terrain normalization requires a non-empty MTEX catalog");

    RequireRawChunk(cell.mcvt, "TVCM", 145u * 4u, "MCVT");
    RequireRawChunk(cell.mcnr, "RNCM", 145u * 3u, "MCNR");
    RequireRawChunk(cell.mcly, "YLCM", static_cast<std::size_t>(cell.header.nLayers) * 16u, "MCLY");

    WotlkTerrainNormalizationResult result;
    TerrainCellInput& terrain = result.terrain;

    for (std::size_t i = 0; i < terrain.heights.size(); ++i)
        terrain.heights[i] = cell.header.y + ReadF32(cell.mcvt, 8u + i * 4u, "MCVT height");

    for (std::size_t i = 0; i < terrain.normals.size(); ++i)
    {
        const std::int8_t bx = static_cast<std::int8_t>(cell.mcnr[8u + i * 3u + 0u]);
        const std::int8_t bz = static_cast<std::int8_t>(cell.mcnr[8u + i * 3u + 1u]);
        const std::int8_t by = static_cast<std::int8_t>(cell.mcnr[8u + i * 3u + 2u]);
        terrain.normals[i] = TerrainNormal{
            static_cast<float>(bx) / 127.0f,
            static_cast<float>(by) / 127.0f,
            static_cast<float>(bz) / 127.0f
        };
    }

    std::vector<RawLayer> rawLayers(cell.header.nLayers);
    terrain.layers.resize(cell.header.nLayers);
    for (std::size_t i = 0; i < rawLayers.size(); ++i)
    {
        const std::size_t at = 8u + i * 16u;
        RawLayer raw;
        raw.textureId = ReadU32(cell.mcly, at + 0u, "MCLY textureId");
        raw.flags = ReadU32(cell.mcly, at + 4u, "MCLY flags");
        raw.alphaOffset = ReadU32(cell.mcly, at + 8u, "MCLY alpha offset");
        raw.effectId = ReadU32(cell.mcly, at + 12u, "MCLY effectId");
        if (raw.textureId >= sourceTextureCount)
            throw std::runtime_error("MCLY textureId exceeds source MTEX catalog");
        rawLayers[i] = raw;

        TerrainLayerInput layer;
        layer.textureId = raw.textureId;
        layer.flags = raw.flags & kTargetSemanticMclyMask;
        layer.effectId = raw.effectId;
        if (i > 0)
            layer.alpha = Alpha8{};
        terrain.layers[i] = std::move(layer);
    }

    if (rawLayers[0].flags & kMclyUseAlpha)
        throw std::runtime_error("WotLK base MCLY layer unexpectedly uses an alpha map");

    bool sawLegacy = false;
    bool sawBig = false;
    bool sawRle = false;
    const bool fixLegacyEdge = (cell.header.flags & kDoNotFixAlphaMap) == 0;

    std::size_t mcalPayloadSize = 0;
    const std::uint8_t* mcalPayload = nullptr;
    if (!cell.mcal.empty())
    {
        if (cell.mcal.size() < 8 || std::memcmp(cell.mcal.data(), "LACM", 4) != 0)
            throw std::runtime_error("MCAL raw chunk header mismatch");
        mcalPayloadSize = ReadU32(cell.mcal, 4u, "MCAL declared size");
        if (mcalPayloadSize + 8u != cell.mcal.size())
            throw std::runtime_error("MCAL declared size disagrees with chunk bytes");
        mcalPayload = cell.mcal.data() + 8u;
    }

    for (std::size_t layerIndex = 1; layerIndex < rawLayers.size(); ++layerIndex)
    {
        const RawLayer& raw = rawLayers[layerIndex];
        if ((raw.flags & kMclyUseAlpha) == 0)
            continue; // Noggit semantics: non-base layer without USE_ALPHA is invisible.
        if (!mcalPayload)
            throw std::runtime_error("MCLY requests alpha data but MCAL is absent");
        if (raw.alphaOffset >= mcalPayloadSize)
            throw std::runtime_error("MCLY alpha offset is outside MCAL payload");

        std::size_t end = mcalPayloadSize;
        for (std::size_t later = layerIndex + 1; later < rawLayers.size(); ++later)
        {
            const std::size_t candidate = rawLayers[later].alphaOffset;
            if (candidate > raw.alphaOffset && candidate < end)
                end = candidate;
        }
        if (end <= raw.alphaOffset)
            throw std::runtime_error("MCLY alpha slice is empty or reversed");
        const std::size_t sliceSize = end - raw.alphaOffset;
        const std::uint8_t* slice = mcalPayload + raw.alphaOffset;

        Alpha8 decoded{};
        if ((raw.flags & kMclyAlphaCompressed) != 0)
        {
            decoded = DecodeRleAlpha8(slice, sliceSize);
            sawRle = true;
            sawBig = true;
        }
        else if (sliceSize == 4096u)
        {
            decoded = DecodeBigAlpha8(slice, sliceSize);
            sawBig = true;
        }
        else if (sliceSize == 2048u)
        {
            decoded = DecodeLegacyAlpha4(slice, sliceSize);
            if (fixLegacyEdge)
                ApplyLegacyEdgeFix(decoded);
            sawLegacy = true;
        }
        else
        {
            const char* hint = sourceWdtBigAlpha ? "WDT marks big-alpha" : "WDT does not mark big-alpha";
            throw std::runtime_error(std::string("unsupported MCAL alpha slice size ") +
                                     std::to_string(sliceSize) + " bytes (" + hint + ")");
        }
        terrain.layers[layerIndex].alpha = decoded;
    }

    if (sawLegacy && sawBig)
        throw std::runtime_error("mixed legacy-4bit and big/RLE alpha storage in one MCNK is not yet normalized safely");

    if (sawLegacy)
    {
        std::vector<Alpha8*> sequential;
        for (std::size_t i = 1; i < terrain.layers.size(); ++i)
        {
            if (rawLayers[i].flags & kMclyUseAlpha)
                sequential.push_back(&*terrain.layers[i].alpha);
        }
        ConvertSequentialToIndependent(sequential);
        result.alphaStorage = WotlkAlphaStorage::Legacy4Sequential;
    }
    else if (sawRle)
    {
        result.alphaStorage = WotlkAlphaStorage::Rle8Independent;
    }
    else if (sawBig)
    {
        result.alphaStorage = WotlkAlphaStorage::Big8Independent;
    }
    else
    {
        result.alphaStorage = WotlkAlphaStorage::None;
    }

    ValidateIndependentAlphaTotals(terrain);
    return result;
}

} // namespace turtle335::adt
