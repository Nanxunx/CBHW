#include "turtle335/adt/McnkWriter.h"

#include "turtle335/adt/MclqWriter.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

constexpr std::size_t kMcnkHeaderPayloadSize = 128;
constexpr std::size_t kMcnkFirstSubchunkOffset = 8 + kMcnkHeaderPayloadSize;
constexpr std::size_t kLegacyMcnrTailSize = 13;
constexpr std::uint32_t kMcseEmitterSize = 0x1Cu;

void WriteU16(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value)
{
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

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
        throw std::invalid_argument("MCNK contains non-finite position");
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, offset, bits);
}

std::uint32_t CheckedU32(std::size_t value, const char* what)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error(std::string(what) + " exceeds uint32 range");
    return static_cast<std::uint32_t>(value);
}

std::vector<std::uint8_t> MakeEmptyChunk(const char rawId[4])
{
    std::vector<std::uint8_t> out(8, 0);
    std::memcpy(out.data(), rawId, 4);
    return out;
}

void ValidateRawChunk(const std::vector<std::uint8_t>& chunk, const char rawId[4], const char* name)
{
    if (chunk.empty())
        return;
    if (chunk.size() < 8)
        throw std::invalid_argument(std::string(name) + " subchunk is shorter than 8 bytes");
    if (std::memcmp(chunk.data(), rawId, 4) != 0)
        throw std::invalid_argument(std::string(name) + " raw FourCC mismatch");

    const std::uint32_t declared =
        static_cast<std::uint32_t>(chunk[4]) |
        (static_cast<std::uint32_t>(chunk[5]) << 8) |
        (static_cast<std::uint32_t>(chunk[6]) << 16) |
        (static_cast<std::uint32_t>(chunk[7]) << 24);
    if (static_cast<std::size_t>(declared) + 8u != chunk.size())
        throw std::invalid_argument(std::string(name) + " declared payload size does not match bytes");
}

std::uint32_t DeclaredPayloadSize(const std::vector<std::uint8_t>& chunk)
{
    if (chunk.size() < 8)
        throw std::invalid_argument("raw ADT chunk is shorter than 8 bytes");
    return static_cast<std::uint32_t>(chunk[4]) |
           (static_cast<std::uint32_t>(chunk[5]) << 8) |
           (static_cast<std::uint32_t>(chunk[6]) << 16) |
           (static_cast<std::uint32_t>(chunk[7]) << 24);
}

void AppendChunk(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& chunk, std::uint32_t& offset)
{
    if (chunk.empty())
    {
        offset = 0;
        return;
    }
    offset = CheckedU32(out.size(), "MCNK subchunk offset");
    out.insert(out.end(), chunk.begin(), chunk.end());
}

} // namespace

SerializedMcnk SerializeVanillaMcnk(const McnkTargetHeader& input, const McnkSubchunks& subchunks)
{
    if (input.ix >= 16 || input.iy >= 16)
        throw std::invalid_argument("MCNK ix/iy must be within 0..15");
    if (input.nLayers > 4)
        throw std::invalid_argument("Vanilla MCNK cannot contain more than 4 texture layers");

    ValidateRawChunk(subchunks.mcvt, "TVCM", "MCVT");
    ValidateRawChunk(subchunks.mcnr, "RNCM", "MCNR");
    ValidateRawChunk(subchunks.mcly, "YLCM", "MCLY");
    ValidateRawChunk(subchunks.mcrf, "FRCM", "MCRF");
    ValidateRawChunk(subchunks.mcsh, "HSCM", "MCSH");
    ValidateRawChunk(subchunks.mcal, "LACM", "MCAL");
    ValidateRawChunk(subchunks.mcse, "ESCM", "MCSE");
    ValidateRawChunk(subchunks.mccv, "VCCM", "MCCV");

    const std::vector<std::uint8_t> canonicalMcrf =
        subchunks.mcrf.empty() ? MakeEmptyChunk("FRCM") : subchunks.mcrf;
    const std::vector<std::uint8_t> canonicalMcse =
        subchunks.mcse.empty() ? MakeEmptyChunk("ESCM") : subchunks.mcse;
    const LegacyMclqBlock canonicalLiquid =
        subchunks.mclq ? *subchunks.mclq : LegacyMclqBlock{};

    const std::uint32_t mcsePayload = DeclaredPayloadSize(canonicalMcse);
    if ((mcsePayload % kMcseEmitterSize) != 0)
        throw std::invalid_argument("MCSE payload is not divisible by the 0x1C emitter size");
    const std::uint32_t nSndEmitters = mcsePayload / kMcseEmitterSize;

    SerializedMcnk result;
    result.bytes.resize(kMcnkFirstSubchunkOffset, 0);
    result.bytes[0] = 'K';
    result.bytes[1] = 'N';
    result.bytes[2] = 'C';
    result.bytes[3] = 'M';

    // Physical-presence flags are writer-owned. Bit15 is semantic: when the
    // caller says full 64x64 alpha/shadow edges are materialized, preserve them
    // by telling Turtle not to synthesize row/column 63 from 62.
    std::uint32_t flags = input.flags & ~(0x01u | 0x3Cu | 0x40u | (1u << 15));
    if (!subchunks.mcsh.empty())
        flags |= 0x01u;
    flags |= canonicalLiquid.mcnkLiquidFlags;
    if (!subchunks.mccv.empty())
        flags |= 0x40u;
    if (input.fullAlphaShadowEdges)
        flags |= (1u << 15);

    AppendChunk(result.bytes, subchunks.mcvt, result.layout.offsMCVT);
    AppendChunk(result.bytes, subchunks.mcnr, result.layout.offsMCNR);
    if (!subchunks.mcnr.empty())
        result.bytes.insert(result.bytes.end(), kLegacyMcnrTailSize, 0);
    AppendChunk(result.bytes, subchunks.mcly, result.layout.offsMCLY);
    AppendChunk(result.bytes, canonicalMcrf, result.layout.offsMCRF);
    AppendChunk(result.bytes, subchunks.mcsh, result.layout.offsMCSH);
    if (!subchunks.mcsh.empty())
        result.layout.sizeMCSH = DeclaredPayloadSize(subchunks.mcsh);
    AppendChunk(result.bytes, subchunks.mcal, result.layout.offsMCAL);
    if (!subchunks.mcal.empty())
        result.layout.sizeMCAL = CheckedU32(subchunks.mcal.size(), "MCAL size");
    AppendChunk(result.bytes, canonicalMcse, result.layout.offsMCSE);

    const std::vector<std::uint8_t> liquid = SerializeLegacyMclqBlock(canonicalLiquid);
    if (liquid.empty())
        throw std::logic_error("canonical old-MCLQ serializer returned no bytes");
    result.layout.offsMCLQ = CheckedU32(result.bytes.size(), "MCLQ offset");
    result.layout.sizeMCLQ = CheckedU32(liquid.size(), "MCLQ size");
    result.bytes.insert(result.bytes.end(), liquid.begin(), liquid.end());

    AppendChunk(result.bytes, subchunks.mccv, result.layout.offsMCCV);

    const std::uint32_t payloadSize = CheckedU32(result.bytes.size() - 8u, "MCNK payload size");
    WriteU32(result.bytes, 4, payloadSize);

    const std::size_t h = 8;
    WriteU32(result.bytes, h + 0, flags);
    WriteU32(result.bytes, h + 4, input.ix);
    WriteU32(result.bytes, h + 8, input.iy);
    WriteU32(result.bytes, h + 12, input.nLayers);
    WriteU32(result.bytes, h + 16, input.nDoodadRefs);
    WriteU32(result.bytes, h + 20, result.layout.offsMCVT);
    WriteU32(result.bytes, h + 24, result.layout.offsMCNR);
    WriteU32(result.bytes, h + 28, result.layout.offsMCLY);
    WriteU32(result.bytes, h + 32, result.layout.offsMCRF);
    WriteU32(result.bytes, h + 36, result.layout.offsMCAL);
    WriteU32(result.bytes, h + 40, result.layout.sizeMCAL);
    WriteU32(result.bytes, h + 44, result.layout.offsMCSH);
    WriteU32(result.bytes, h + 48, result.layout.sizeMCSH);
    WriteU32(result.bytes, h + 52, input.areaId);
    WriteU32(result.bytes, h + 56, input.nMapObjRefs);
    WriteU16(result.bytes, h + 60, input.holes);
    WriteU16(result.bytes, h + 62, input.legacy3E);
    std::memcpy(result.bytes.data() + h + 64, input.lowQualityTextureMap.data(), input.lowQualityTextureMap.size());
    WriteU32(result.bytes, h + 80, input.predTex);
    WriteU32(result.bytes, h + 84, input.nEffectDoodad);
    WriteU32(result.bytes, h + 88, result.layout.offsMCSE);
    WriteU32(result.bytes, h + 92, nSndEmitters);
    WriteU32(result.bytes, h + 96, result.layout.offsMCLQ);
    WriteU32(result.bytes, h + 100, result.layout.sizeMCLQ);
    WriteF32(result.bytes, h + 104, input.z);
    WriteF32(result.bytes, h + 108, input.x);
    WriteF32(result.bytes, h + 112, input.y);
    WriteU32(result.bytes, h + 116, result.layout.offsMCCV);
    WriteU32(result.bytes, h + 120, input.props);
    WriteU32(result.bytes, h + 124, input.effectId);

    return result;
}

} // namespace turtle335::adt
