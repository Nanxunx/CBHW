#include "turtle335/adt/Mh2oReader.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

std::uint16_t ReadU16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t ReadU32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t ReadU64(const std::uint8_t* p) noexcept
{
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(p[i]) << (i * 8);
    return value;
}

float ReadF32(const std::uint8_t* p)
{
    const std::uint32_t bits = ReadU32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value))
        throw std::runtime_error("MH2O contains non-finite float");
    return value;
}

void RequireRange(std::size_t offset, std::size_t length, std::size_t payloadSize, const char* what)
{
    if (offset > payloadSize || length > payloadSize - offset)
        throw std::runtime_error(std::string("MH2O out-of-range ") + what);
}

} // namespace

Mh2oParseResult ParseMh2oChunk(const std::uint8_t* data, std::size_t size, const LiquidTypeResolver& resolver)
{
    if (!data || size < 8)
        throw std::invalid_argument("MH2O chunk is missing or truncated");
    // ADT chunk FourCCs are stored reversed on disk: logical MH2O -> raw O2HM.
    if (data[0] != 'O' || data[1] != '2' || data[2] != 'H' || data[3] != 'M')
        throw std::invalid_argument("expected raw O2HM (logical MH2O) chunk");

    const std::size_t declaredPayloadSize = ReadU32(data + 4);
    if (declaredPayloadSize > size - 8)
        throw std::runtime_error("MH2O declared size exceeds supplied bytes");
    if (declaredPayloadSize < 256u * 12u)
        throw std::runtime_error("MH2O payload is smaller than the 256-entry header table");

    const std::uint8_t* payload = data + 8;
    const std::size_t payloadSize = declaredPayloadSize;

    Mh2oParseResult result;

    for (std::size_t chunkIndex = 0; chunkIndex < 256; ++chunkIndex)
    {
        const std::uint8_t* entry = payload + chunkIndex * 12;
        const std::uint32_t instancesOffset = ReadU32(entry + 0);
        const std::uint32_t layerCount = ReadU32(entry + 4);
        const std::uint32_t attributesOffset = ReadU32(entry + 8);

        if (layerCount == 0)
            continue;
        if (instancesOffset == 0)
            throw std::runtime_error("MH2O layer count is nonzero but instance offset is zero");
        if (layerCount > 64)
            throw std::runtime_error("MH2O layer count is implausibly large");

        RequireRange(instancesOffset, static_cast<std::size_t>(layerCount) * 24u, payloadSize, "instance table");

        std::uint64_t fishable = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t deep = std::numeric_limits<std::uint64_t>::max();
        if (attributesOffset != 0)
        {
            RequireRange(attributesOffset, 16, payloadSize, "attributes");
            fishable = ReadU64(payload + attributesOffset);
            deep = ReadU64(payload + attributesOffset + 8);
        }

        for (std::size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            const std::uint8_t* src = payload + instancesOffset + layerIndex * 24;
            const std::uint16_t liquidType = ReadU16(src + 0);
            const std::uint16_t rawFormat = ReadU16(src + 2);
            if (rawFormat > static_cast<std::uint16_t>(Mh2oVertexFormat::Depth))
                throw std::runtime_error("unsupported build-12340 MH2O vertex format");

            const Mh2oVertexFormat format = static_cast<Mh2oVertexFormat>(rawFormat);
            const float minHeight = ReadF32(src + 4);
            const float maxHeight = ReadF32(src + 8);
            const std::uint8_t offsetX = src[12];
            const std::uint8_t offsetY = src[13];
            const std::uint8_t width = src[14];
            const std::uint8_t height = src[15];
            const std::uint32_t existsOffset = ReadU32(src + 16);
            const std::uint32_t vertexDataOffset = ReadU32(src + 20);

            if (width == 0 || height == 0 || offsetX + width > 8 || offsetY + height > 8)
                throw std::runtime_error("MH2O instance rectangle is outside 8x8 MCNK cells");

            ParsedMh2oLayer parsed;
            parsed.metadata.sourceLiquidType = liquidType;
            parsed.metadata.sourceVertexFormat = format;
            parsed.metadata.minHeightLevel = minHeight;
            parsed.metadata.maxHeightLevel = maxHeight;

            LiquidLayer& layer = parsed.layer;
            layer.category = resolver ? resolver(liquidType) : LiquidCategory::Unknown;
            layer.offsetX = offsetX;
            layer.offsetY = offsetY;
            layer.width = width;
            layer.height = height;
            layer.fishableMask = fishable;
            layer.deepMask = deep;

            std::uint64_t exists = std::numeric_limits<std::uint64_t>::max();
            if (existsOffset != 0)
            {
                RequireRange(existsOffset, 8, payloadSize, "exists bitmap");
                exists = ReadU64(payload + existsOffset);
            }

            layer.visible.resize(static_cast<std::size_t>(width) * height);
            for (std::size_t i = 0; i < layer.visible.size(); ++i)
                layer.visible[i] = ((exists >> i) & 1u) != 0;

            const std::size_t vertexCount = static_cast<std::size_t>(width + 1) * (height + 1);
            layer.vertices.resize(vertexCount);

            if (vertexDataOffset == 0)
            {
                for (LiquidVertex& vertex : layer.vertices)
                {
                    vertex.height = minHeight;
                    if (format == Mh2oVertexFormat::Depth || format == Mh2oVertexFormat::HeightDepth)
                        vertex.depth = 0;
                }
            }
            else if (format == Mh2oVertexFormat::HeightDepth)
            {
                const std::size_t heightsBytes = vertexCount * 4u;
                const std::size_t depthsBytes = vertexCount;
                RequireRange(vertexDataOffset, heightsBytes + depthsBytes, payloadSize, "HeightDepth vertex data");
                const std::uint8_t* heights = payload + vertexDataOffset;
                const std::uint8_t* depths = heights + heightsBytes;
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    layer.vertices[i].height = ReadF32(heights + i * 4u);
                    layer.vertices[i].depth = depths[i];
                }
            }
            else if (format == Mh2oVertexFormat::HeightTextureCoord)
            {
                const std::size_t heightsBytes = vertexCount * 4u;
                const std::size_t uvBytes = vertexCount * 4u;
                RequireRange(vertexDataOffset, heightsBytes + uvBytes, payloadSize, "HeightTextureCoord vertex data");
                const std::uint8_t* heights = payload + vertexDataOffset;
                const std::uint8_t* uv = heights + heightsBytes;
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    layer.vertices[i].height = ReadF32(heights + i * 4u);
                    layer.vertices[i].u = ReadU16(uv + i * 4u);
                    layer.vertices[i].v = ReadU16(uv + i * 4u + 2u);
                }
            }
            else // Depth
            {
                RequireRange(vertexDataOffset, vertexCount, payloadSize, "Depth vertex data");
                const std::uint8_t* depths = payload + vertexDataOffset;
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    layer.vertices[i].height = minHeight;
                    layer.vertices[i].depth = depths[i];
                }
            }

            result.chunks[chunkIndex].push_back(std::move(parsed));
        }
    }

    return result;
}

} // namespace turtle335::adt
