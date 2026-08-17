#include "turtle335/adt/AdtValidator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace turtle335::adt {
namespace {

struct ChunkView
{
    std::size_t offset = 0;
    std::size_t payloadOffset = 0;
    std::size_t payloadSize = 0;
    std::size_t totalSize = 0;
};

void RequireRange(std::size_t offset, std::size_t size, std::size_t total, const char* what)
{
    if (offset > total || size > total - offset)
        throw std::runtime_error(std::string(what) + " is outside file range");
}

std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    RequireRange(offset, 2, bytes.size(), what);
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    RequireRange(offset, 4, bytes.size(), what);
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

void RequireId(const std::vector<std::uint8_t>& bytes,
               std::size_t offset,
               const char rawId[4],
               const char* what)
{
    RequireRange(offset, 4, bytes.size(), what);
    if (std::memcmp(bytes.data() + offset, rawId, 4) != 0)
        throw std::runtime_error(std::string(what) + " FourCC mismatch");
}

ChunkView GetChunk(const std::vector<std::uint8_t>& bytes,
                   std::size_t offset,
                   const char rawId[4],
                   const char* what)
{
    RequireRange(offset, 8, bytes.size(), what);
    RequireId(bytes, offset, rawId, what);
    const std::size_t payload = ReadU32(bytes, offset + 4, what);
    RequireRange(offset, 8u + payload, bytes.size(), what);
    return {offset, offset + 8u, payload, 8u + payload};
}

std::size_t ResolveMhdr(const std::vector<std::uint8_t>& bytes,
                        std::size_t fieldOffset,
                        const char rawId[4],
                        const char* what,
                        bool required)
{
    constexpr std::size_t mhdrPayload = 20;
    const std::uint32_t relative = ReadU32(bytes, mhdrPayload + fieldOffset, what);
    if (relative == 0)
    {
        if (required)
            throw std::runtime_error(std::string(what) + " MHDR pointer is zero");
        return 0;
    }
    const std::size_t absolute = mhdrPayload + static_cast<std::size_t>(relative);
    RequireId(bytes, absolute, rawId, what);
    return absolute;
}

std::vector<std::size_t> ParseStringStarts(const std::vector<std::uint8_t>& bytes,
                                           const ChunkView& chunk,
                                           const char* what)
{
    std::vector<std::size_t> starts;
    std::size_t cursor = 0;
    while (cursor < chunk.payloadSize)
    {
        starts.push_back(cursor);
        std::size_t end = cursor;
        while (end < chunk.payloadSize && bytes[chunk.payloadOffset + end] != 0)
            ++end;
        if (end == chunk.payloadSize)
            throw std::runtime_error(std::string(what) + " string table is not NUL-terminated");
        if (end == cursor)
            throw std::runtime_error(std::string(what) + " contains empty path string");
        cursor = end + 1u;
    }
    return starts;
}

std::size_t ValidatePathCatalog(const std::vector<std::uint8_t>& bytes,
                                std::size_t stringOffset,
                                const char stringId[4],
                                std::size_t indexOffset,
                                const char indexId[4],
                                const char* label)
{
    if (stringOffset == 0 && indexOffset == 0)
        return 0;
    if (stringOffset == 0 || indexOffset == 0)
        throw std::runtime_error(std::string(label) + " string/index chunk pair is incomplete");

    const ChunkView strings = GetChunk(bytes, stringOffset, stringId, label);
    const ChunkView index = GetChunk(bytes, indexOffset, indexId, label);
    if ((index.payloadSize % 4u) != 0)
        throw std::runtime_error(std::string(label) + " index payload is not divisible by four");

    const std::vector<std::size_t> starts = ParseStringStarts(bytes, strings, label);
    const std::unordered_set<std::size_t> validStarts(starts.begin(), starts.end());
    const std::size_t count = index.payloadSize / 4u;
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t relative = ReadU32(bytes, index.payloadOffset + i * 4u, label);
        if (validStarts.find(relative) == validStarts.end())
            throw std::runtime_error(std::string(label) + " index points into the middle/outside of string table");
    }
    return count;
}

std::size_t CountLiquidFlags(std::uint32_t flags)
{
    std::size_t count = 0;
    for (const std::uint32_t bit : {0x04u, 0x08u, 0x10u, 0x20u})
        if ((flags & bit) != 0)
            ++count;
    return count;
}

void ValidateBounds(const std::vector<std::uint8_t>& bytes, std::size_t minOffset, std::size_t maxOffset)
{
    const float minX = ReadF32(bytes, minOffset + 0, "MODF minimum");
    const float minY = ReadF32(bytes, minOffset + 4, "MODF minimum");
    const float minZ = ReadF32(bytes, minOffset + 8, "MODF minimum");
    const float maxX = ReadF32(bytes, maxOffset + 0, "MODF maximum");
    const float maxY = ReadF32(bytes, maxOffset + 4, "MODF maximum");
    const float maxZ = ReadF32(bytes, maxOffset + 8, "MODF maximum");
    if (minX > maxX || minY > maxY || minZ > maxZ)
        throw std::runtime_error("MODF bounds are reversed");
}

void ValidateVector(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    (void)ReadF32(bytes, offset + 0, what);
    (void)ReadF32(bytes, offset + 4, what);
    (void)ReadF32(bytes, offset + 8, what);
}

} // namespace

AdtValidationReport ValidateVanillaAdt(const std::vector<std::uint8_t>& bytes)
{
    AdtValidationReport report;
    report.fileSize = bytes.size();

    if (bytes.size() < 4196)
        throw std::runtime_error("ADT is too small for canonical root and MCIN");

    const ChunkView mver = GetChunk(bytes, 0, "REVM", "MVER");
    if (mver.payloadSize != 4)
        throw std::runtime_error("MVER payload must be four bytes");
    report.version = ReadU32(bytes, mver.payloadOffset, "MVER version");
    if (report.version != 18)
        throw std::runtime_error("canonical target ADT MVER must be 18");

    const ChunkView mhdr = GetChunk(bytes, 12, "RDHM", "MHDR");
    if (mhdr.payloadSize != 64)
        throw std::runtime_error("MHDR payload must be 64 bytes");

    const std::size_t mcinOffset = ResolveMhdr(bytes, 4, "NICM", "MCIN", true);
    if (mcinOffset != 84)
        throw std::runtime_error("canonical MCIN must begin at file byte 84");
    const ChunkView mcin = GetChunk(bytes, mcinOffset, "NICM", "MCIN");
    if (mcin.payloadSize != 256u * 16u)
        throw std::runtime_error("MCIN payload must contain 256 16-byte entries");

    const std::size_t mtexOffset = ResolveMhdr(bytes, 8, "XETM", "MTEX", true);
    const ChunkView mtex = GetChunk(bytes, mtexOffset, "XETM", "MTEX");
    report.textureCount = ParseStringStarts(bytes, mtex, "MTEX").size();
    if (report.textureCount == 0)
        throw std::runtime_error("canonical terrain ADT must contain at least one MTEX path");

    const std::size_t mmdxOffset = ResolveMhdr(bytes, 12, "XDMM", "MMDX", false);
    const std::size_t mmidOffset = ResolveMhdr(bytes, 16, "DIMM", "MMID", false);
    const std::size_t mwmoOffset = ResolveMhdr(bytes, 20, "OMWM", "MWMO", false);
    const std::size_t mwidOffset = ResolveMhdr(bytes, 24, "DIWM", "MWID", false);
    const std::size_t mddfOffset = ResolveMhdr(bytes, 28, "FDDM", "MDDF", false);
    const std::size_t modfOffset = ResolveMhdr(bytes, 32, "FDOM", "MODF", false);
    const std::uint32_t mh2oRelative = ReadU32(bytes, 20 + 40, "MH2O pointer");
    if (mh2oRelative != 0)
        throw std::runtime_error("canonical Turtle client ADT must not retain MH2O");

    report.m2PathCount = ValidatePathCatalog(bytes, mmdxOffset, "XDMM", mmidOffset, "DIMM", "M2 path catalog");
    report.wmoPathCount = ValidatePathCatalog(bytes, mwmoOffset, "OMWM", mwidOffset, "DIWM", "WMO path catalog");

    std::unordered_set<std::uint32_t> uniqueIds;

    if (mddfOffset != 0)
    {
        const ChunkView mddf = GetChunk(bytes, mddfOffset, "FDDM", "MDDF");
        if ((mddf.payloadSize % 36u) != 0)
            throw std::runtime_error("MDDF payload is not divisible by 36 bytes");
        report.m2PlacementCount = mddf.payloadSize / 36u;
        for (std::size_t i = 0; i < report.m2PlacementCount; ++i)
        {
            const std::size_t at = mddf.payloadOffset + i * 36u;
            const std::uint32_t nameId = ReadU32(bytes, at + 0, "MDDF NameId");
            const std::uint32_t uid = ReadU32(bytes, at + 4, "MDDF UniqueId");
            if (nameId >= report.m2PathCount)
                throw std::runtime_error("MDDF NameId exceeds MMID count");
            if (uid == 0 || !uniqueIds.insert(uid).second)
                throw std::runtime_error("MDDF/MODF UniqueId is zero or duplicated");
            ValidateVector(bytes, at + 8, "MDDF position");
            ValidateVector(bytes, at + 20, "MDDF rotation");
            if (ReadU16(bytes, at + 32, "MDDF scale") == 0)
                throw std::runtime_error("MDDF scale cannot be zero");
        }
    }

    if (modfOffset != 0)
    {
        const ChunkView modf = GetChunk(bytes, modfOffset, "FDOM", "MODF");
        if ((modf.payloadSize % 64u) != 0)
            throw std::runtime_error("MODF payload is not divisible by 64 bytes");
        report.wmoPlacementCount = modf.payloadSize / 64u;
        for (std::size_t i = 0; i < report.wmoPlacementCount; ++i)
        {
            const std::size_t at = modf.payloadOffset + i * 64u;
            const std::uint32_t nameId = ReadU32(bytes, at + 0, "MODF NameId");
            const std::uint32_t uid = ReadU32(bytes, at + 4, "MODF UniqueId");
            if (nameId >= report.wmoPathCount)
                throw std::runtime_error("MODF NameId exceeds MWID count");
            if (uid == 0 || !uniqueIds.insert(uid).second)
                throw std::runtime_error("MDDF/MODF UniqueId is zero or duplicated");
            ValidateVector(bytes, at + 8, "MODF position");
            ValidateVector(bytes, at + 20, "MODF rotation");
            ValidateBounds(bytes, at + 32, at + 44);
        }
    }

    std::vector<std::pair<std::size_t, std::size_t>> mcnkRanges;
    mcnkRanges.reserve(256);

    for (std::size_t slot = 0; slot < 256; ++slot)
    {
        const std::size_t entry = mcin.payloadOffset + slot * 16u;
        const std::size_t mcnkOffset = ReadU32(bytes, entry + 0, "MCIN MCNK offset");
        const std::size_t mcnkSize = ReadU32(bytes, entry + 4, "MCIN MCNK size");
        if (mcnkOffset == 0 || mcnkSize < 136)
            throw std::runtime_error("MCIN contains missing/short MCNK");
        RequireRange(mcnkOffset, mcnkSize, bytes.size(), "MCIN MCNK");
        RequireId(bytes, mcnkOffset, "KNCM", "MCNK");
        if (static_cast<std::size_t>(ReadU32(bytes, mcnkOffset + 4, "MCNK size")) + 8u != mcnkSize)
            throw std::runtime_error("MCIN MCNK size disagrees with MCNK chunk header");

        const std::uint32_t flags = ReadU32(bytes, mcnkOffset + 8, "MCNK flags");
        const std::uint32_t ix = ReadU32(bytes, mcnkOffset + 12, "MCNK ix");
        const std::uint32_t iy = ReadU32(bytes, mcnkOffset + 16, "MCNK iy");
        const std::uint32_t nLayers = ReadU32(bytes, mcnkOffset + 20, "MCNK nLayers");
        const std::uint32_t nDoodadRefs = ReadU32(bytes, mcnkOffset + 24, "MCNK nDoodadRefs");
        const std::uint32_t nMapObjRefs = ReadU32(bytes, mcnkOffset + 64, "MCNK nMapObjRefs");

        if (ix != slot % 16u || iy != slot / 16u)
            throw std::runtime_error("MCIN y-major slot disagrees with MCNK ix/iy");
        if (nLayers == 0 || nLayers > 4)
            throw std::runtime_error("canonical terrain MCNK must have 1..4 layers");
        if ((flags & (1u << 15)) != 0)
            throw std::runtime_error("canonical old-MCLQ MCNK must clear do_not_fix_alpha_map bit15");

        const std::size_t ofsMcvt = ReadU32(bytes, mcnkOffset + 28, "MCNK offsMCVT");
        const std::size_t ofsMcnr = ReadU32(bytes, mcnkOffset + 32, "MCNK offsMCNR");
        const std::size_t ofsMcly = ReadU32(bytes, mcnkOffset + 36, "MCNK offsMCLY");
        const std::size_t ofsMcrf = ReadU32(bytes, mcnkOffset + 40, "MCNK offsMCRF");
        const std::size_t ofsMcal = ReadU32(bytes, mcnkOffset + 44, "MCNK offsMCAL");
        const std::uint32_t sizeMcal = ReadU32(bytes, mcnkOffset + 48, "MCNK sizeMCAL");
        const std::size_t ofsMcsh = ReadU32(bytes, mcnkOffset + 52, "MCNK offsMCSH");
        const std::uint32_t sizeMcsh = ReadU32(bytes, mcnkOffset + 56, "MCNK sizeMCSH");
        const std::size_t ofsMcse = ReadU32(bytes, mcnkOffset + 96, "MCNK offsMCSE");
        const std::uint32_t nSndEmitters = ReadU32(bytes, mcnkOffset + 100, "MCNK nSndEmitters");
        const std::size_t ofsMclq = ReadU32(bytes, mcnkOffset + 104, "MCNK offsMCLQ");
        const std::uint32_t sizeMclq = ReadU32(bytes, mcnkOffset + 108, "MCNK sizeMCLQ");
        const std::size_t ofsMccv = ReadU32(bytes, mcnkOffset + 124, "MCNK offsMCCV");

        const ChunkView mcvt = GetChunk(bytes, mcnkOffset + ofsMcvt, "TVCM", "MCVT");
        if (mcvt.payloadSize != 145u * 4u)
            throw std::runtime_error("MCVT payload must be 580 bytes");

        const ChunkView mcnr = GetChunk(bytes, mcnkOffset + ofsMcnr, "RNCM", "MCNR");
        if (mcnr.payloadSize != 145u * 3u)
            throw std::runtime_error("MCNR payload must be 435 bytes");
        if (ofsMcnr != ofsMcvt + mcvt.totalSize)
            throw std::runtime_error("canonical MCNR does not immediately follow MCVT");
        if (ofsMcly != ofsMcnr + mcnr.totalSize + 13u)
            throw std::runtime_error("canonical MCLY does not account for 13-byte MCNR tail");

        const ChunkView mcly = GetChunk(bytes, mcnkOffset + ofsMcly, "YLCM", "MCLY");
        if (mcly.payloadSize != static_cast<std::size_t>(nLayers) * 16u)
            throw std::runtime_error("MCLY payload size disagrees with nLayers");

        for (std::size_t layer = 0; layer < nLayers; ++layer)
        {
            const std::size_t at = mcly.payloadOffset + layer * 16u;
            const std::uint32_t textureId = ReadU32(bytes, at + 0, "MCLY textureId");
            const std::uint32_t layerFlags = ReadU32(bytes, at + 4, "MCLY flags");
            const std::uint32_t alphaOffset = ReadU32(bytes, at + 8, "MCLY alpha offset");
            if (textureId >= report.textureCount)
                throw std::runtime_error("MCLY textureId exceeds MTEX texture count");
            if (layer == 0)
            {
                if ((layerFlags & 0x300u) != 0 || alphaOffset != 0)
                    throw std::runtime_error("base MCLY layer must not use alpha/compression");
            }
            else
            {
                if ((layerFlags & 0x100u) == 0 || (layerFlags & 0x200u) != 0)
                    throw std::runtime_error("non-base MCLY layer must use uncompressed old alpha");
                const std::uint32_t expected = static_cast<std::uint32_t>((layer - 1u) * 2048u);
                if (alphaOffset != expected)
                    throw std::runtime_error("MCLY alpha offset does not match canonical 2048-byte old-alpha layout");
            }
        }

        const ChunkView mcrf = GetChunk(bytes, mcnkOffset + ofsMcrf, "FRCM", "MCRF");
        const std::size_t refCount = static_cast<std::size_t>(nDoodadRefs) + nMapObjRefs;
        if (mcrf.payloadSize != refCount * 4u)
            throw std::runtime_error("MCRF payload size disagrees with MCNK reference counts");
        for (std::size_t i = 0; i < nDoodadRefs; ++i)
            if (ReadU32(bytes, mcrf.payloadOffset + i * 4u, "MCRF M2 ref") >= report.m2PlacementCount)
                throw std::runtime_error("MCRF M2 ref exceeds MDDF count");
        for (std::size_t i = 0; i < nMapObjRefs; ++i)
            if (ReadU32(bytes, mcrf.payloadOffset + (nDoodadRefs + i) * 4u, "MCRF WMO ref") >= report.wmoPlacementCount)
                throw std::runtime_error("MCRF WMO ref exceeds MODF count");

        const ChunkView mcal = GetChunk(bytes, mcnkOffset + ofsMcal, "LACM", "MCAL");
        const std::size_t expectedAlphaPayload = static_cast<std::size_t>(nLayers - 1u) * 2048u;
        if (mcal.payloadSize != expectedAlphaPayload || sizeMcal != mcal.totalSize)
            throw std::runtime_error("MCAL size does not match canonical old-alpha layer count");

        const bool hasShadow = (flags & 0x01u) != 0;
        if (hasShadow)
        {
            if (ofsMcsh == 0 || sizeMcsh != 512)
                throw std::runtime_error("MCNK shadow flag requires MCSH payload size 512");
            const ChunkView mcsh = GetChunk(bytes, mcnkOffset + ofsMcsh, "HSCM", "MCSH");
            if (mcsh.payloadSize != 512)
                throw std::runtime_error("MCSH payload must be 512 bytes");
        }
        else if (ofsMcsh != 0 || sizeMcsh != 0)
            throw std::runtime_error("MCNK without shadow flag must not point to MCSH");

        if (ofsMcse == 0)
            throw std::runtime_error("canonical MCNK must retain MCSE header");
        const ChunkView mcse = GetChunk(bytes, mcnkOffset + ofsMcse, "ESCM", "MCSE");
        if ((mcse.payloadSize % 0x1Cu) != 0 || mcse.payloadSize / 0x1Cu != nSndEmitters)
            throw std::runtime_error("MCSE payload/count mismatch");
        report.soundEmitterCount += nSndEmitters;

        if (ofsMclq == 0 || sizeMclq < 8)
            throw std::runtime_error("canonical old-MCLQ MCNK must retain QLCM header");
        RequireRange(mcnkOffset + ofsMclq, sizeMclq, bytes.size(), "MCLQ block");
        RequireId(bytes, mcnkOffset + ofsMclq, "QLCM", "MCLQ");
        if (ReadU32(bytes, mcnkOffset + ofsMclq + 4, "MCLQ inner size") != 0)
            throw std::runtime_error("canonical MCLQ inner size field must be zero");
        const std::size_t liquidRecords = CountLiquidFlags(flags);
        if (sizeMclq != 8u + liquidRecords * 804u)
            throw std::runtime_error("MCNK sizeMCLQ disagrees with liquid category record count");
        report.liquidRecordCount += liquidRecords;

        const bool hasMccv = (flags & 0x40u) != 0;
        if (hasMccv)
        {
            if (ofsMccv == 0)
                throw std::runtime_error("MCNK MCCV flag set but offset is zero");
            const ChunkView mccv = GetChunk(bytes, mcnkOffset + ofsMccv, "VCCM", "MCCV");
            if (mccv.payloadSize != 145u * 4u)
                throw std::runtime_error("MCCV payload must be 580 bytes");
        }
        else if (ofsMccv != 0)
            throw std::runtime_error("MCNK MCCV offset set while flag bit6 is clear");

        mcnkRanges.emplace_back(mcnkOffset, mcnkOffset + mcnkSize);
        ++report.mcnkCount;
    }

    std::sort(mcnkRanges.begin(), mcnkRanges.end());
    for (std::size_t i = 1; i < mcnkRanges.size(); ++i)
        if (mcnkRanges[i - 1].second > mcnkRanges[i].first)
            throw std::runtime_error("MCIN MCNK ranges overlap");

    if (report.mcnkCount != 256)
        throw std::runtime_error("ADT must contain exactly 256 MCNK entries");

    return report;
}

} // namespace turtle335::adt
