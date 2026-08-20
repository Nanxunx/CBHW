#include "turtle335/adt/WotlkWdtReader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside WDT bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool IdEquals(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char rawId[4])
{
    return std::memcmp(bytes.data() + offset, rawId, 4) == 0;
}

} // namespace

WotlkWdtDocument ParseWotlkWdt(const std::vector<std::uint8_t>& bytes)
{
    WotlkWdtDocument result;
    bool haveMver = false;
    bool haveMphd = false;
    bool haveMain = false;

    std::size_t cursor = 0;
    while (cursor < bytes.size())
    {
        if (bytes.size() - cursor < 8u)
            throw std::runtime_error("WDT has a truncated chunk header");
        const std::uint32_t payloadSize = ReadU32(bytes, cursor + 4u, "WDT chunk size");
        const std::size_t totalSize = 8u + static_cast<std::size_t>(payloadSize);
        if (totalSize > bytes.size() - cursor)
            throw std::runtime_error("WDT chunk extends outside file");
        const std::size_t payload = cursor + 8u;

        if (IdEquals(bytes, cursor, "REVM"))
        {
            if (haveMver)
                throw std::runtime_error("WDT contains duplicate MVER chunks");
            if (payloadSize != 4u)
                throw std::runtime_error("WDT MVER payload must be four bytes");
            result.version = ReadU32(bytes, payload, "WDT MVER version");
            haveMver = true;
        }
        else if (IdEquals(bytes, cursor, "DHPM"))
        {
            if (haveMphd)
                throw std::runtime_error("WDT contains duplicate MPHD chunks");
            if (payloadSize != 32u)
                throw std::runtime_error("WDT MPHD payload must contain eight uint32 values");
            for (std::size_t i = 0; i < result.mphd.size(); ++i)
                result.mphd[i] = ReadU32(bytes, payload + i * 4u, "WDT MPHD field");
            haveMphd = true;
        }
        else if (IdEquals(bytes, cursor, "NIAM"))
        {
            if (haveMain)
                throw std::runtime_error("WDT contains duplicate MAIN chunks");
            constexpr std::size_t expected = 64u * 64u * 8u;
            if (payloadSize != expected)
                throw std::runtime_error("WDT MAIN payload must contain 4096 eight-byte tile entries");
            for (std::size_t slot = 0; slot < result.tiles.size(); ++slot)
            {
                const std::size_t at = payload + slot * 8u;
                result.tiles[slot].flags = ReadU32(bytes, at + 0u, "WDT MAIN flags");
                result.tiles[slot].asyncId = ReadU32(bytes, at + 4u, "WDT MAIN asyncId");
            }
            haveMain = true;
        }

        cursor += totalSize;
    }

    if (!haveMver || !haveMphd || !haveMain)
        throw std::runtime_error("WotLK WDT requires MVER, MPHD and MAIN chunks");
    if (result.version != 18u)
        throw std::runtime_error("WotLK WDT reader requires MVER 18");

    result.globalWmo = (result.mphd[0] & 0x01u) != 0;
    result.bigAlpha = (result.mphd[0] & 0x04u) != 0;
    return result;
}

WdtWriterInput NormalizeWotlkTerrainWdt(const WotlkWdtDocument& source)
{
    if (source.version != 18u)
        throw std::invalid_argument("WDT normalizer requires source MVER 18");
    if (source.globalWmo)
        throw std::runtime_error("global-WMO source WDT is outside the terrain-only target profile");

    WdtWriterInput target;
    for (std::size_t slot = 0; slot < source.tiles.size(); ++slot)
    {
        target.tiles[slot].flags = source.tiles[slot].flags & 1u;
        target.tiles[slot].asyncId = 0;
    }
    return target;
}

} // namespace turtle335::adt
