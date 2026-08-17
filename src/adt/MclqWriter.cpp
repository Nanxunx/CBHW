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

} // namespace

LegacyMclqPayloadBytes SerializeLegacyMclqPayload(const LegacyMclq& mclq)
{
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
    bytes[0] = 'M';
    bytes[1] = 'C';
    bytes[2] = 'L';
    bytes[3] = 'Q';

    std::uint8_t* sizeOut = bytes.data() + 4;
    WriteU32(sizeOut, 804u);

    const LegacyMclqPayloadBytes payload = SerializeLegacyMclqPayload(mclq);
    std::memcpy(bytes.data() + 8, payload.data(), payload.size());
    return bytes;
}

} // namespace turtle335::adt
