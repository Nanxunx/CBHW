#include "turtle335/m2/SkinViewWriter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace turtle335::m2 {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& data, const std::size_t offset, const char* what)
{
    if (offset > data.size() || 4u > data.size() - offset)
        throw std::runtime_error(std::string(what) + " outside skin");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void WriteU32(std::vector<std::uint8_t>& out, const std::size_t offset, const std::uint32_t value)
{
    if (offset > out.size() || 4u > out.size() - offset)
        throw std::out_of_range("SkinViewWriter patch outside output");
    out[offset] = static_cast<std::uint8_t>(value & 0xffu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

void CheckSpan(const std::vector<std::uint8_t>& data, const std::uint32_t offset,
               const std::size_t size, const char* what)
{
    const std::size_t at = static_cast<std::size_t>(offset);
    if (at > data.size() || size > data.size() - at)
        throw std::runtime_error(std::string(what) + " outside skin");
}

std::uint32_t CheckedAbsolute(const std::uint32_t base, const std::size_t relative)
{
    const std::uint64_t value = static_cast<std::uint64_t>(base) + relative;
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("embedded View absolute offset exceeds uint32");
    return static_cast<std::uint32_t>(value);
}

std::uint32_t AppendAligned(std::vector<std::uint8_t>& out, const std::uint32_t absoluteBase,
                            const std::uint8_t* data, const std::size_t size,
                            const std::size_t alignment = 4u)
{
    if (size == 0u)
        return 0u;
    if (alignment > 1u)
    {
        const std::size_t padding = (alignment - (out.size() % alignment)) % alignment;
        out.insert(out.end(), padding, 0u);
    }
    const std::uint32_t absolute = CheckedAbsolute(absoluteBase, out.size());
    out.insert(out.end(), data, data + size);
    return absolute;
}

} // namespace

std::vector<std::uint8_t> BuildClassicEmbeddedViews(
    const std::vector<std::vector<std::uint8_t>>& skinFiles,
    const std::uint32_t targetAbsoluteOffset)
{
    const std::size_t headerBytes = skinFiles.size() * static_cast<std::size_t>(kClassicViewHeaderSize);
    std::vector<std::uint8_t> out(headerBytes, 0u);

    for (std::size_t viewIndex = 0; viewIndex < skinFiles.size(); ++viewIndex)
    {
        const auto& skin = skinFiles[viewIndex];
        if (skin.size() < kWotlkSkinHeaderSize || std::memcmp(skin.data(), "SKIN", 4u) != 0)
            throw std::runtime_error("external skin is not a valid WotLK SKIN header");

        const std::uint32_t nIndices = ReadU32(skin, 4u, "nIndices");
        const std::uint32_t oIndices = ReadU32(skin, 8u, "ofsIndices");
        const std::uint32_t nTriangles = ReadU32(skin, 12u, "nTriangles");
        const std::uint32_t oTriangles = ReadU32(skin, 16u, "ofsTriangles");
        const std::uint32_t nProperties = ReadU32(skin, 20u, "nProperties");
        const std::uint32_t oProperties = ReadU32(skin, 24u, "ofsProperties");
        const std::uint32_t nSubmeshes = ReadU32(skin, 28u, "nSubmeshes");
        const std::uint32_t oSubmeshes = ReadU32(skin, 32u, "ofsSubmeshes");
        const std::uint32_t nTextureUnits = ReadU32(skin, 36u, "nTextureUnits");
        const std::uint32_t oTextureUnits = ReadU32(skin, 40u, "ofsTextureUnits");
        const std::uint32_t lod = ReadU32(skin, 44u, "LOD");

        const std::size_t indicesBytes = static_cast<std::size_t>(nIndices) * 2u;
        const std::size_t trianglesBytes = static_cast<std::size_t>(nTriangles) * 2u;
        const std::size_t propertiesBytes = static_cast<std::size_t>(nProperties) * 4u;
        const std::size_t submeshBytes = static_cast<std::size_t>(nSubmeshes) * kWotlkSkinSubmeshSize;
        const std::size_t textureUnitBytes = static_cast<std::size_t>(nTextureUnits) * kSkinTextureUnitSize;

        CheckSpan(skin, oIndices, indicesBytes, "indices");
        CheckSpan(skin, oTriangles, trianglesBytes, "triangles");
        CheckSpan(skin, oProperties, propertiesBytes, "properties");
        CheckSpan(skin, oSubmeshes, submeshBytes, "submeshes");
        CheckSpan(skin, oTextureUnits, textureUnitBytes, "texture units");

        const std::size_t header = viewIndex * static_cast<std::size_t>(kClassicViewHeaderSize);

        const auto indicesOut = AppendAligned(out, targetAbsoluteOffset, skin.data() + oIndices, indicesBytes);
        const auto trianglesOut = AppendAligned(out, targetAbsoluteOffset, skin.data() + oTriangles, trianglesBytes);
        const auto propertiesOut = AppendAligned(out, targetAbsoluteOffset, skin.data() + oProperties, propertiesBytes);

        std::vector<std::uint8_t> classicSubmeshes(
            static_cast<std::size_t>(nSubmeshes) * kClassicViewSubmeshSize, 0u);
        for (std::uint32_t i = 0; i < nSubmeshes; ++i)
        {
            const std::size_t source = static_cast<std::size_t>(oSubmeshes) +
                                       static_cast<std::size_t>(i) * kWotlkSkinSubmeshSize;
            const std::size_t target = static_cast<std::size_t>(i) * kClassicViewSubmeshSize;
            std::copy_n(
                skin.begin() + static_cast<std::ptrdiff_t>(source),
                kClassicViewSubmeshSize,
                classicSubmeshes.begin() + static_cast<std::ptrdiff_t>(target));
        }
        const auto submeshesOut = AppendAligned(
            out, targetAbsoluteOffset, classicSubmeshes.data(), classicSubmeshes.size());
        const auto textureUnitsOut = AppendAligned(
            out, targetAbsoluteOffset, skin.data() + oTextureUnits, textureUnitBytes);

        WriteU32(out, header + 0u, nIndices);
        WriteU32(out, header + 4u, indicesOut);
        WriteU32(out, header + 8u, nTriangles);
        WriteU32(out, header + 12u, trianglesOut);
        WriteU32(out, header + 16u, nProperties);
        WriteU32(out, header + 20u, propertiesOut);
        WriteU32(out, header + 24u, nSubmeshes);
        WriteU32(out, header + 28u, submeshesOut);
        WriteU32(out, header + 32u, nTextureUnits);
        WriteU32(out, header + 36u, textureUnitsOut);
        WriteU32(out, header + 40u, lod);
    }

    return out;
}

} // namespace turtle335::m2
