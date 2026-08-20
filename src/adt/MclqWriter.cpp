#include "turtle335/adt/MclqWriter.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace turtle335::adt {
namespace {

void WriteU32(std::uint8_t*& out, std::uint32_t value) noexcept
{
    *out++ = static_cast<std::uint8_t>(value & 0xFFu);
    *out++ = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    *out++ = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    *out++ = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

void WriteF32(std::uint8_t*& out, float value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("MCLQ contains non-finite float");

    static_assert(sizeof(float) == sizeof(std::uint32_t), "32-bit float required");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, bits);
}

LiquidCategory ExpectedCategory(std::size_t slot)
{
    constexpr std::array<LiquidCategory, 4> categories = {
        LiquidCategory::Water,
        LiquidCategory::Ocean,
        LiquidCategory::Magma,
        LiquidCategory::Slime
    };
    return categories.at(slot);
}

} // namespace

LegacyMclqPayloadBytes SerializeLegacyMclqPayload(const LegacyMclq& mclq)
{
    if (!SlotForCategory(mclq.category))
        throw std::invalid_argument("MCLQ record has unknown category");
    if (!std::isfinite(mclq.minHeight) || !std::isfinite(mclq.maxHeight) || mclq.minHeight > mclq.maxHeight)
        throw std::invalid_argument("invalid MCLQ min/max height");

    LegacyMclqPayloadBytes bytes{};
    std::uint8_t* out = bytes.data();

    WriteF32(out, mclq.minHeight);
    WriteF32(out, mclq.maxHeight);

    for (const LegacyMclqVertex& vertex : mclq.vertices)
    {
        WriteU32(out, vertex.lightOrUv);
        WriteF32(out, vertex.height);
    }

    for (std::uint8_t flag : mclq.cellFlags)
        *out++ = flag;

    for (std::uint8_t value : mclq.flowData)
        *out++ = value;

    if (out != bytes.data() + bytes.size())
        throw std::logic_error("MCLQ payload serializer size mismatch");

    return bytes;
}

LegacyMclqChunkBytes SerializeLegacyMclqChunk(const LegacyMclq& mclq)
{
    LegacyMclqChunkBytes bytes{};
    bytes[0] = 'Q';
    bytes[1] = 'L';
    bytes[2] = 'C';
    bytes[3] = 'M';
    // Known-good Vanilla/Noggit convention: inner MCLQ chunk size is zero;
    // MCNK.sizeMCLQ carries the complete 8 + 804*N size.
    std::uint8_t* sizeOut = bytes.data() + 4;
    WriteU32(sizeOut, 0u);

    const LegacyMclqPayloadBytes payload = SerializeLegacyMclqPayload(mclq);
    std::memcpy(bytes.data() + 8, payload.data(), payload.size());
    return bytes;
}

std::vector<std::uint8_t> SerializeLegacyMclqBlock(const LegacyMclqBlock& block)
{
    const std::size_t count = LegacyMclqRecordCount(block);
    if (count == 0)
    {
        if (block.mcnkLiquidFlags != 0)
            throw std::invalid_argument("empty MCLQ block cannot advertise MCNK liquid flags");
        // Noggit/Vanilla convention for a dry MCNK in old-MCLQ mode:
        // keep an 8-byte QLCM header with inner size 0 and MCNK.sizeLiquid=8.
        std::vector<std::uint8_t> bytes(8, 0);
        bytes[0] = 'Q';
        bytes[1] = 'L';
        bytes[2] = 'C';
        bytes[3] = 'M';
        return bytes;
    }

    std::vector<std::uint8_t> bytes(8 + count * LegacyMclqPayloadBytes{}.size(), 0);
    bytes[0] = 'Q';
    bytes[1] = 'L';
    bytes[2] = 'C';
    bytes[3] = 'M';
    std::uint8_t* sizeOut = bytes.data() + 4;
    WriteU32(sizeOut, 0u);

    std::size_t outOffset = 8;
    std::uint32_t expectedFlags = 0;
    for (std::size_t slot = 0; slot < block.records.size(); ++slot)
    {
        if (!block.records[slot])
            continue;

        const LiquidCategory expected = ExpectedCategory(slot);
        if (block.records[slot]->category != expected)
            throw std::invalid_argument("MCLQ block record category does not match fixed slot");

        const LegacyMclqPayloadBytes payload = SerializeLegacyMclqPayload(*block.records[slot]);
        std::memcpy(bytes.data() + outOffset, payload.data(), payload.size());
        outOffset += payload.size();
        expectedFlags |= McnkLiquidFlag(expected);
    }

    if (expectedFlags != block.mcnkLiquidFlags)
        throw std::invalid_argument("MCLQ block flags do not match record presence");
    if (outOffset != bytes.size())
        throw std::logic_error("MCLQ block serializer size mismatch");

    return bytes;
}

} // namespace turtle335::adt
