#include "turtle335/adt/TerrainWriter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kFlagUseAlpha = 0x100u;
constexpr std::uint32_t kFlagAlphaCompressed = 0x200u;
constexpr std::uint32_t kKnownMclyFlagMask = 0x7FFu;

void WriteU32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value)
{
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

void WriteF32(std::vector<std::uint8_t>& out, std::size_t offset, float value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("terrain contains non-finite height");
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, offset, bits);
}

std::vector<std::uint8_t> MakeChunk(const char rawId[4], const std::vector<std::uint8_t>& payload)
{
    if (payload.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("terrain subchunk payload exceeds uint32 range");
    std::vector<std::uint8_t> out(8u + payload.size(), 0);
    std::memcpy(out.data(), rawId, 4);
    WriteU32(out, 4, static_cast<std::uint32_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), out.begin() + 8);
    return out;
}

std::int8_t QuantizeNormal(float value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("terrain normal contains non-finite component");
    value = std::clamp(value, -1.0f, 1.0f);
    const int encoded = static_cast<int>(value * 127.0f); // Noggit-compatible truncation.
    return static_cast<std::int8_t>(std::clamp(encoded, -127, 127));
}

std::array<Alpha8, 3> ConvertBigContributionsToOldSequential(const TerrainCellInput& input)
{
    std::array<Alpha8, 3> old{};
    const std::size_t alphaCount = input.layers.size() - 1u;

    for (std::size_t layer = 0; layer < alphaCount; ++layer)
    {
        if (!input.layers[layer + 1u].alpha)
            throw std::invalid_argument("every non-base terrain layer requires an alpha map");
        old[layer] = *input.layers[layer + 1u].alpha;
    }

    // Noggit's alphas_to_old_alpha conversion. Normalized big-alpha values are
    // actual layer contributions; legacy alpha values are sequential coverage.
    for (std::size_t pixel = 0; pixel < Alpha8{}.size(); ++pixel)
    {
        unsigned total = 0;
        for (std::size_t layer = 0; layer < alphaCount; ++layer)
            total += old[layer][pixel];
        if (total > 255u)
            throw std::invalid_argument("normalized terrain alpha contributions exceed 255 at one pixel");

        int remaining = 255;
        for (std::size_t reverse = alphaCount; reverse-- > 0; )
        {
            const int current = old[reverse][pixel];
            if (remaining <= 0)
            {
                old[reverse][pixel] = 0;
                continue;
            }
            const int sequential = (current * 255 + remaining / 2) / remaining;
            old[reverse][pixel] = static_cast<std::uint8_t>(std::clamp(sequential, 0, 255));
            remaining -= current;
        }
    }
    return old;
}

} // namespace

SerializedTerrain SerializeLegacyTerrain(const TerrainCellInput& input,
                                         std::size_t textureCount)
{
    if (input.layers.empty() || input.layers.size() > 4)
        throw std::invalid_argument("Vanilla terrain cell must contain 1..4 texture layers");
    if (textureCount == 0)
        throw std::invalid_argument("terrain layers require a non-empty MTEX catalog");

    for (std::size_t i = 0; i < input.layers.size(); ++i)
    {
        const TerrainLayerInput& layer = input.layers[i];
        if (layer.textureId >= textureCount)
            throw std::out_of_range("MCLY textureId exceeds MTEX texture count");
        if ((layer.flags & ~kKnownMclyFlagMask) != 0)
            throw std::invalid_argument("MCLY contains target-unverified flag bits");
        if (i == 0 && layer.alpha)
            throw std::invalid_argument("base terrain layer must not carry an alpha map");
        if (i > 0 && !layer.alpha)
            throw std::invalid_argument("non-base terrain layer is missing alpha map");
    }

    SerializedTerrain out;
    out.baseHeight = input.heights[0];
    if (!std::isfinite(out.baseHeight))
        throw std::invalid_argument("terrain base height is non-finite");
    out.nLayers = static_cast<std::uint32_t>(input.layers.size());

    // MCVT: 145 relative heights. Noggit subtracts vertex 0 and stores that
    // absolute baseline in MCNK.ypos.
    std::vector<std::uint8_t> mcvtPayload(145u * 4u, 0);
    for (std::size_t i = 0; i < input.heights.size(); ++i)
    {
        if (!std::isfinite(input.heights[i]))
            throw std::invalid_argument("terrain contains non-finite height");
        WriteF32(mcvtPayload, i * 4u, input.heights[i] - out.baseHeight);
    }
    out.mcvt = MakeChunk("TVCM", mcvtPayload);

    // MCNR: declared payload is exactly 145*3=435 bytes. McnkWriter appends
    // the canonical 13-byte legacy tail outside the MCNR declared size.
    std::vector<std::uint8_t> mcnrPayload(145u * 3u, 0);
    for (std::size_t i = 0; i < input.normals.size(); ++i)
    {
        const TerrainNormal& source = input.normals[i];
        if (!std::isfinite(source.x) || !std::isfinite(source.y) || !std::isfinite(source.z))
            throw std::invalid_argument("terrain normal contains non-finite component");
        const float lengthSquared = source.x * source.x + source.y * source.y + source.z * source.z;
        if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared))
            throw std::invalid_argument("terrain normal cannot be zero length");
        const float invLength = 1.0f / std::sqrt(lengthSquared);
        const float x = source.x * invLength;
        const float y = source.y * invLength;
        const float z = source.z * invLength;

        // Noggit disk order: x, z, y. Reader reconstructs x, byte2, byte1.
        mcnrPayload[i * 3u + 0] = static_cast<std::uint8_t>(QuantizeNormal(x));
        mcnrPayload[i * 3u + 1] = static_cast<std::uint8_t>(QuantizeNormal(z));
        mcnrPayload[i * 3u + 2] = static_cast<std::uint8_t>(QuantizeNormal(y));
    }
    out.mcnr = MakeChunk("RNCM", mcnrPayload);

    const std::size_t alphaCount = input.layers.size() - 1u;
    const std::array<Alpha8, 3> oldAlpha = ConvertBigContributionsToOldSequential(input);
    std::vector<std::uint8_t> mcalPayload(alphaCount * Alpha4Packed{}.size(), 0);
    for (std::size_t layer = 0; layer < alphaCount; ++layer)
    {
        const Alpha4Packed packed = EncodeLegacyAlpha4(oldAlpha[layer]);
        std::copy(packed.begin(), packed.end(),
                  mcalPayload.begin() + static_cast<std::ptrdiff_t>(layer * packed.size()));
    }
    out.mcal = MakeChunk("LACM", mcalPayload);

    std::vector<std::uint8_t> mclyPayload(input.layers.size() * 16u, 0);
    std::uint32_t alphaOffset = 0;
    for (std::size_t i = 0; i < input.layers.size(); ++i)
    {
        const TerrainLayerInput& layer = input.layers[i];
        const std::size_t at = i * 16u;
        std::uint32_t flags = layer.flags & ~(kFlagUseAlpha | kFlagAlphaCompressed);
        if (i > 0)
            flags |= kFlagUseAlpha;

        WriteU32(mclyPayload, at + 0, layer.textureId);
        WriteU32(mclyPayload, at + 4, flags);
        WriteU32(mclyPayload, at + 8, alphaOffset);
        WriteU32(mclyPayload, at + 12, layer.effectId);
        if (i > 0)
            alphaOffset += static_cast<std::uint32_t>(Alpha4Packed{}.size());
    }
    out.mcly = MakeChunk("YLCM", mclyPayload);

    return out;
}

void ApplyTerrainToMcnk(const SerializedTerrain& terrain,
                        McnkTargetHeader& header,
                        McnkSubchunks& subchunks)
{
    if (terrain.nLayers == 0 || terrain.nLayers > 4)
        throw std::invalid_argument("serialized terrain has invalid layer count");
    header.nLayers = terrain.nLayers;
    header.y = terrain.baseHeight;
    subchunks.mcvt = terrain.mcvt;
    subchunks.mcnr = terrain.mcnr;
    subchunks.mcly = terrain.mcly;
    subchunks.mcal = terrain.mcal;
}

} // namespace turtle335::adt
