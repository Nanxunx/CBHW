#include "turtle335/adt/WdtWriter.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kVersion = 18;
constexpr std::size_t kMverPayload = 4;
constexpr std::size_t kMphdPayload = 32;
constexpr std::size_t kMainPayload = 64u * 64u * 8u;
constexpr std::size_t kExpectedFileSize = (8u + kMverPayload) + (8u + kMphdPayload) + (8u + kMainPayload);
constexpr std::size_t kMainChunkOffset = 12u + 40u;
constexpr std::size_t kMainPayloadOffset = kMainChunkOffset + 8u;

void WriteU32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value)
{
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside WDT bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void WriteChunkHeader(std::vector<std::uint8_t>& out,
                      std::size_t offset,
                      const char rawId[4],
                      std::uint32_t payloadSize)
{
    std::memcpy(out.data() + offset, rawId, 4);
    WriteU32(out, offset + 4, payloadSize);
}

void RequireChunk(const std::vector<std::uint8_t>& bytes,
                  std::size_t offset,
                  const char rawId[4],
                  std::uint32_t expectedPayload,
                  const char* what)
{
    if (offset > bytes.size() || 8u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " header is outside WDT bytes");
    if (std::memcmp(bytes.data() + offset, rawId, 4) != 0)
        throw std::runtime_error(std::string(what) + " FourCC mismatch");
    if (ReadU32(bytes, offset + 4, what) != expectedPayload)
        throw std::runtime_error(std::string(what) + " payload size mismatch");
    if (8u + static_cast<std::size_t>(expectedPayload) > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " extends outside WDT bytes");
}

} // namespace

void SetWdtTerrainTile(WdtWriterInput& input,
                       std::uint32_t x,
                       std::uint32_t y,
                       bool present)
{
    if (x >= 64 || y >= 64)
        throw std::out_of_range("WDT tile coordinate must be within 0..63");
    WdtTileEntry& entry = input.tiles[WdtTileSlot(x, y)];
    if (present)
        entry.flags |= 1u;
    else
        entry.flags &= ~1u;
}

std::vector<std::uint8_t> SerializeTerrainWdt(const WdtWriterInput& input)
{
    if ((input.mphd[0] & 1u) != 0)
        throw std::invalid_argument("global-WMO WDT is outside terrain-only writer profile");

    std::vector<std::uint8_t> bytes(kExpectedFileSize, 0);

    WriteChunkHeader(bytes, 0, "REVM", static_cast<std::uint32_t>(kMverPayload));
    WriteU32(bytes, 8, kVersion);

    constexpr std::size_t mphdOffset = 12;
    WriteChunkHeader(bytes, mphdOffset, "DHPM", static_cast<std::uint32_t>(kMphdPayload));
    for (std::size_t i = 0; i < input.mphd.size(); ++i)
        WriteU32(bytes, mphdOffset + 8u + i * 4u, input.mphd[i]);

    WriteChunkHeader(bytes, kMainChunkOffset, "NIAM", static_cast<std::uint32_t>(kMainPayload));
    for (std::size_t slot = 0; slot < input.tiles.size(); ++slot)
    {
        const WdtTileEntry& entry = input.tiles[slot];
        const std::size_t at = kMainPayloadOffset + slot * 8u;
        WriteU32(bytes, at + 0, entry.flags);
        WriteU32(bytes, at + 4, entry.asyncId);
    }

    (void)ValidateTerrainWdt(bytes);
    return bytes;
}

WdtValidationReport ValidateTerrainWdt(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() != kExpectedFileSize)
        throw std::runtime_error("canonical terrain WDT must contain exactly MVER+MPHD+MAIN");

    RequireChunk(bytes, 0, "REVM", static_cast<std::uint32_t>(kMverPayload), "MVER");
    RequireChunk(bytes, 12, "DHPM", static_cast<std::uint32_t>(kMphdPayload), "MPHD");
    RequireChunk(bytes, kMainChunkOffset, "NIAM", static_cast<std::uint32_t>(kMainPayload), "MAIN");

    WdtValidationReport report;
    report.fileSize = bytes.size();
    report.version = ReadU32(bytes, 8, "MVER version");
    if (report.version != kVersion)
        throw std::runtime_error("terrain WDT MVER must be 18");

    report.headerFlags = ReadU32(bytes, 20, "MPHD flags");
    if ((report.headerFlags & 1u) != 0)
        throw std::runtime_error("global-WMO MPHD flag is unsupported by terrain WDT validator");

    for (std::size_t slot = 0; slot < 64u * 64u; ++slot)
    {
        const std::size_t at = kMainPayloadOffset + slot * 8u;
        const std::uint32_t flags = ReadU32(bytes, at + 0, "MAIN flags");
        (void)ReadU32(bytes, at + 4, "MAIN asyncId");
        if ((flags & 1u) != 0)
            ++report.presentTiles;
    }
    return report;
}

} // namespace turtle335::adt
