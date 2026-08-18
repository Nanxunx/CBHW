#include "turtle335/adt/WotlkAdtReader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
        throw std::runtime_error(std::string(what) + " is outside ADT bytes");
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

AdtVec3 ReadVec3(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    return {
        ReadF32(bytes, offset + 0u, what),
        ReadF32(bytes, offset + 4u, what),
        ReadF32(bytes, offset + 8u, what)
    };
}

void RequireId(const std::vector<std::uint8_t>& bytes,
               std::size_t offset,
               const char rawId[4],
               const char* what)
{
    RequireRange(offset, 4, bytes.size(), what);
    if (std::memcmp(bytes.data() + offset, rawId, 4) != 0)
        throw std::runtime_error(std::string(what) + " raw FourCC mismatch");
}

ChunkView GetChunk(const std::vector<std::uint8_t>& bytes,
                   std::size_t offset,
                   const char rawId[4],
                   const char* what)
{
    RequireRange(offset, 8, bytes.size(), what);
    RequireId(bytes, offset, rawId, what);
    const std::size_t payloadSize = ReadU32(bytes, offset + 4u, what);
    RequireRange(offset, 8u + payloadSize, bytes.size(), what);
    return {offset, offset + 8u, payloadSize, 8u + payloadSize};
}

std::vector<std::uint8_t> CopyChunk(const std::vector<std::uint8_t>& bytes,
                                    std::size_t offset,
                                    const char rawId[4],
                                    const char* what)
{
    if (offset == 0)
        return {};
    const ChunkView chunk = GetChunk(bytes, offset, rawId, what);
    return std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset + chunk.totalSize));
}

std::vector<std::uint8_t> CopySubchunk(const std::vector<std::uint8_t>& bytes,
                                       const ChunkView& mcnk,
                                       std::uint32_t relativeOffset,
                                       const char rawId[4],
                                       const char* what)
{
    if (relativeOffset == 0)
        return {};
    const std::size_t absolute = mcnk.offset + static_cast<std::size_t>(relativeOffset);
    if (absolute < mcnk.payloadOffset + 128u || absolute >= mcnk.offset + mcnk.totalSize)
        throw std::runtime_error(std::string(what) + " offset is outside MCNK payload data");
    const ChunkView chunk = GetChunk(bytes, absolute, rawId, what);
    if (chunk.offset + chunk.totalSize > mcnk.offset + mcnk.totalSize)
        throw std::runtime_error(std::string(what) + " extends outside owning MCNK");
    return std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset + chunk.totalSize));
}

std::size_t ResolveMhdr(const std::vector<std::uint8_t>& bytes,
                        const ChunkView& mhdr,
                        std::size_t fieldOffset,
                        const char rawId[4],
                        const char* what,
                        bool required)
{
    const std::uint32_t relative = ReadU32(bytes, mhdr.payloadOffset + fieldOffset, what);
    if (relative == 0)
    {
        if (required)
            throw std::runtime_error(std::string(what) + " MHDR pointer is zero");
        return 0;
    }
    const std::size_t absolute = mhdr.payloadOffset + static_cast<std::size_t>(relative);
    (void)GetChunk(bytes, absolute, rawId, what);
    return absolute;
}

std::vector<std::string> ReadSequentialStrings(const std::vector<std::uint8_t>& bytes,
                                               const ChunkView& chunk,
                                               const char* what)
{
    std::vector<std::string> result;
    std::size_t cursor = 0;
    while (cursor < chunk.payloadSize)
    {
        std::size_t end = cursor;
        while (end < chunk.payloadSize && bytes[chunk.payloadOffset + end] != 0)
            ++end;
        if (end == chunk.payloadSize)
            throw std::runtime_error(std::string(what) + " string table is not NUL terminated");
        if (end == cursor)
            throw std::runtime_error(std::string(what) + " contains an empty path");
        result.emplace_back(reinterpret_cast<const char*>(bytes.data() + chunk.payloadOffset + cursor),
                            end - cursor);
        cursor = end + 1u;
    }
    return result;
}

std::unordered_map<std::uint32_t, std::string> ReadOffsetStringTable(const std::vector<std::uint8_t>& bytes,
                                                                     const ChunkView& chunk,
                                                                     const char* what)
{
    std::unordered_map<std::uint32_t, std::string> result;
    std::size_t cursor = 0;
    while (cursor < chunk.payloadSize)
    {
        const std::size_t start = cursor;
        std::size_t end = cursor;
        while (end < chunk.payloadSize && bytes[chunk.payloadOffset + end] != 0)
            ++end;
        if (end == chunk.payloadSize)
            throw std::runtime_error(std::string(what) + " string table is not NUL terminated");
        if (end > start)
        {
            result.emplace(static_cast<std::uint32_t>(start),
                           std::string(reinterpret_cast<const char*>(bytes.data() + chunk.payloadOffset + start),
                                       end - start));
        }
        cursor = end + 1u;
    }
    return result;
}

std::vector<std::uint32_t> ReadIndexTable(const std::vector<std::uint8_t>& bytes,
                                          const ChunkView& chunk,
                                          const char* what)
{
    if ((chunk.payloadSize % 4u) != 0)
        throw std::runtime_error(std::string(what) + " payload is not divisible by four");
    std::vector<std::uint32_t> result(chunk.payloadSize / 4u);
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = ReadU32(bytes, chunk.payloadOffset + i * 4u, what);
    return result;
}

std::string ResolveCatalogPath(std::uint32_t nameId,
                               const std::vector<std::uint32_t>& offsets,
                               const std::unordered_map<std::uint32_t, std::string>& strings,
                               const char* what)
{
    if (nameId >= offsets.size())
        throw std::runtime_error(std::string(what) + " NameId exceeds index table");
    const auto found = strings.find(offsets[nameId]);
    if (found == strings.end())
        throw std::runtime_error(std::string(what) + " index does not point to a path start");
    return found->second;
}

} // namespace

WotlkAdtDocument ParseWotlkAdt(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < 12u + 72u)
        throw std::runtime_error("ADT is too small for MVER and MHDR");

    WotlkAdtDocument result;

    const ChunkView mver = GetChunk(bytes, 0, "REVM", "MVER");
    if (mver.payloadSize != 4)
        throw std::runtime_error("MVER payload must be four bytes");
    result.version = ReadU32(bytes, mver.payloadOffset, "MVER version");
    if (result.version != 18)
        throw std::runtime_error("WotLK ADT reader requires MVER 18");

    const ChunkView mhdr = GetChunk(bytes, mver.totalSize, "RDHM", "MHDR");
    if (mhdr.payloadSize != 64)
        throw std::runtime_error("MHDR payload must be 64 bytes");

    const std::size_t mcinOffset = ResolveMhdr(bytes, mhdr, 4, "NICM", "MCIN", true);
    const ChunkView mcin = GetChunk(bytes, mcinOffset, "NICM", "MCIN");
    if (mcin.payloadSize != 256u * 16u)
        throw std::runtime_error("MCIN payload must contain exactly 256 entries");

    const std::size_t mtexOffset = ResolveMhdr(bytes, mhdr, 8, "XETM", "MTEX", false);
    if (mtexOffset != 0)
        result.textures = ReadSequentialStrings(bytes, GetChunk(bytes, mtexOffset, "XETM", "MTEX"), "MTEX");

    const std::size_t mmdxOffset = ResolveMhdr(bytes, mhdr, 12, "XDMM", "MMDX", false);
    const std::size_t mmidOffset = ResolveMhdr(bytes, mhdr, 16, "DIMM", "MMID", false);
    const std::size_t mwmoOffset = ResolveMhdr(bytes, mhdr, 20, "OMWM", "MWMO", false);
    const std::size_t mwidOffset = ResolveMhdr(bytes, mhdr, 24, "DIWM", "MWID", false);
    const std::size_t mddfOffset = ResolveMhdr(bytes, mhdr, 28, "FDDM", "MDDF", false);
    const std::size_t modfOffset = ResolveMhdr(bytes, mhdr, 32, "FDOM", "MODF", false);
    const std::size_t mfboOffset = ResolveMhdr(bytes, mhdr, 36, "OBFM", "MFBO", false);
    const std::size_t mh2oOffset = ResolveMhdr(bytes, mhdr, 40, "O2HM", "MH2O", false);

    if (mfboOffset != 0)
        result.mfbo = CopyChunk(bytes, mfboOffset, "OBFM", "MFBO");
    if (mh2oOffset != 0)
        result.mh2o = CopyChunk(bytes, mh2oOffset, "O2HM", "MH2O");

    std::unordered_map<std::uint32_t, std::string> m2Strings;
    std::vector<std::uint32_t> m2Offsets;
    if (mmdxOffset != 0 || mmidOffset != 0)
    {
        if (mmdxOffset == 0 || mmidOffset == 0)
            throw std::runtime_error("MMDX/MMID catalog pair is incomplete");
        m2Strings = ReadOffsetStringTable(bytes, GetChunk(bytes, mmdxOffset, "XDMM", "MMDX"), "MMDX");
        m2Offsets = ReadIndexTable(bytes, GetChunk(bytes, mmidOffset, "DIMM", "MMID"), "MMID");
    }

    std::unordered_map<std::uint32_t, std::string> wmoStrings;
    std::vector<std::uint32_t> wmoOffsets;
    if (mwmoOffset != 0 || mwidOffset != 0)
    {
        if (mwmoOffset == 0 || mwidOffset == 0)
            throw std::runtime_error("MWMO/MWID catalog pair is incomplete");
        wmoStrings = ReadOffsetStringTable(bytes, GetChunk(bytes, mwmoOffset, "OMWM", "MWMO"), "MWMO");
        wmoOffsets = ReadIndexTable(bytes, GetChunk(bytes, mwidOffset, "DIWM", "MWID"), "MWID");
    }

    if (mddfOffset != 0)
    {
        if (mmdxOffset == 0 || mmidOffset == 0)
            throw std::runtime_error("MDDF exists without MMDX/MMID path catalog");
        const ChunkView mddf = GetChunk(bytes, mddfOffset, "FDDM", "MDDF");
        if ((mddf.payloadSize % 36u) != 0)
            throw std::runtime_error("MDDF payload is not divisible by 36 bytes");
        result.m2Placements.reserve(mddf.payloadSize / 36u);
        for (std::size_t i = 0; i < mddf.payloadSize / 36u; ++i)
        {
            const std::size_t at = mddf.payloadOffset + i * 36u;
            M2PlacementInput placement;
            placement.assetPath = ResolveCatalogPath(ReadU32(bytes, at + 0, "MDDF NameId"), m2Offsets, m2Strings, "MDDF");
            placement.uniqueId = ReadU32(bytes, at + 4, "MDDF UniqueId");
            placement.position = ReadVec3(bytes, at + 8, "MDDF position");
            placement.rotation = ReadVec3(bytes, at + 20, "MDDF rotation");
            placement.scale = ReadU16(bytes, at + 32, "MDDF scale");
            placement.flags = ReadU16(bytes, at + 34, "MDDF flags");
            result.m2Placements.push_back(std::move(placement));
        }
    }

    if (modfOffset != 0)
    {
        if (mwmoOffset == 0 || mwidOffset == 0)
            throw std::runtime_error("MODF exists without MWMO/MWID path catalog");
        const ChunkView modf = GetChunk(bytes, modfOffset, "FDOM", "MODF");
        if ((modf.payloadSize % 64u) != 0)
            throw std::runtime_error("MODF payload is not divisible by 64 bytes");
        result.wmoPlacements.reserve(modf.payloadSize / 64u);
        for (std::size_t i = 0; i < modf.payloadSize / 64u; ++i)
        {
            const std::size_t at = modf.payloadOffset + i * 64u;
            WmoPlacementInput placement;
            placement.assetPath = ResolveCatalogPath(ReadU32(bytes, at + 0, "MODF NameId"), wmoOffsets, wmoStrings, "MODF");
            placement.uniqueId = ReadU32(bytes, at + 4, "MODF UniqueId");
            placement.position = ReadVec3(bytes, at + 8, "MODF position");
            placement.rotation = ReadVec3(bytes, at + 20, "MODF rotation");
            placement.minimumExtent = ReadVec3(bytes, at + 32, "MODF minimum extent");
            placement.maximumExtent = ReadVec3(bytes, at + 44, "MODF maximum extent");
            placement.flags = ReadU16(bytes, at + 56, "MODF flags");
            placement.doodadSet = ReadU16(bytes, at + 58, "MODF doodad set");
            placement.nameSet = ReadU16(bytes, at + 60, "MODF name set");
            placement.scale = ReadU16(bytes, at + 62, "MODF scale");
            result.wmoPlacements.push_back(std::move(placement));
        }
    }

    std::array<bool, 256> occupied{};
    for (std::size_t entryIndex = 0; entryIndex < 256; ++entryIndex)
    {
        const std::size_t entry = mcin.payloadOffset + entryIndex * 16u;
        const std::size_t mcnkOffset = ReadU32(bytes, entry + 0, "MCIN MCNK offset");
        const std::size_t mcnkSize = ReadU32(bytes, entry + 4, "MCIN MCNK size");
        if (mcnkOffset == 0 || mcnkSize < 136u)
            throw std::runtime_error("MCIN contains missing/short MCNK entry");
        const ChunkView mcnk = GetChunk(bytes, mcnkOffset, "KNCM", "MCNK");
        if (mcnk.totalSize != mcnkSize)
            throw std::runtime_error("MCIN MCNK size disagrees with MCNK chunk header");
        if (mcnk.payloadSize < 128u)
            throw std::runtime_error("MCNK payload is shorter than the 128-byte header");

        const std::uint32_t ix = ReadU32(bytes, mcnkOffset + 12u, "MCNK ix");
        const std::uint32_t iy = ReadU32(bytes, mcnkOffset + 16u, "MCNK iy");
        if (ix >= 16 || iy >= 16)
            throw std::runtime_error("MCNK ix/iy is outside 0..15");
        const std::size_t slot = static_cast<std::size_t>(iy) * 16u + static_cast<std::size_t>(ix);
        if (occupied[slot])
            throw std::runtime_error("ADT contains duplicate MCNK ix/iy");
        occupied[slot] = true;

        WotlkMcnkRecord& cell = result.cells[slot];
        cell.header.flags = ReadU32(bytes, mcnkOffset + 8u, "MCNK flags");
        cell.header.ix = ix;
        cell.header.iy = iy;
        cell.header.nLayers = ReadU32(bytes, mcnkOffset + 20u, "MCNK nLayers");
        cell.header.nDoodadRefs = ReadU32(bytes, mcnkOffset + 24u, "MCNK nDoodadRefs");
        cell.header.areaId = ReadU32(bytes, mcnkOffset + 60u, "MCNK areaId");
        cell.header.nMapObjRefs = ReadU32(bytes, mcnkOffset + 64u, "MCNK nMapObjRefs");
        cell.header.holes = ReadU16(bytes, mcnkOffset + 68u, "MCNK holes");
        cell.header.legacy3E = ReadU16(bytes, mcnkOffset + 70u, "MCNK +0x3E");
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(mcnkOffset + 72u),
                    16u,
                    cell.header.lowQualityTextureMap.begin());
        cell.header.predTex = ReadU32(bytes, mcnkOffset + 88u, "MCNK predTex");
        cell.header.nEffectDoodad = ReadU32(bytes, mcnkOffset + 92u, "MCNK nEffectDoodad");
        cell.header.nSndEmitters = ReadU32(bytes, mcnkOffset + 100u, "MCNK nSndEmitters");
        cell.header.z = ReadF32(bytes, mcnkOffset + 112u, "MCNK zpos");
        cell.header.x = ReadF32(bytes, mcnkOffset + 116u, "MCNK xpos");
        cell.header.y = ReadF32(bytes, mcnkOffset + 120u, "MCNK ypos");
        cell.header.props = ReadU32(bytes, mcnkOffset + 128u, "MCNK props");
        cell.header.effectId = ReadU32(bytes, mcnkOffset + 132u, "MCNK effectId");

        const std::uint32_t offsMCVT = ReadU32(bytes, mcnkOffset + 28u, "MCNK offsMCVT");
        const std::uint32_t offsMCNR = ReadU32(bytes, mcnkOffset + 32u, "MCNK offsMCNR");
        const std::uint32_t offsMCLY = ReadU32(bytes, mcnkOffset + 36u, "MCNK offsMCLY");
        const std::uint32_t offsMCRF = ReadU32(bytes, mcnkOffset + 40u, "MCNK offsMCRF");
        const std::uint32_t offsMCAL = ReadU32(bytes, mcnkOffset + 44u, "MCNK offsMCAL");
        const std::uint32_t offsMCSH = ReadU32(bytes, mcnkOffset + 52u, "MCNK offsMCSH");
        const std::uint32_t offsMCSE = ReadU32(bytes, mcnkOffset + 96u, "MCNK offsMCSE");
        const std::uint32_t offsMCLQ = ReadU32(bytes, mcnkOffset + 104u, "MCNK offsMCLQ");
        const std::uint32_t offsMCCV = ReadU32(bytes, mcnkOffset + 124u, "MCNK offsMCCV");

        cell.mcvt = CopySubchunk(bytes, mcnk, offsMCVT, "TVCM", "MCVT");
        cell.mcnr = CopySubchunk(bytes, mcnk, offsMCNR, "RNCM", "MCNR");
        cell.mcly = CopySubchunk(bytes, mcnk, offsMCLY, "YLCM", "MCLY");
        cell.mcal = CopySubchunk(bytes, mcnk, offsMCAL, "LACM", "MCAL");
        cell.mcsh = CopySubchunk(bytes, mcnk, offsMCSH, "HSCM", "MCSH");
        cell.mcse = CopySubchunk(bytes, mcnk, offsMCSE, "ESCM", "MCSE");
        cell.mclq = CopySubchunk(bytes, mcnk, offsMCLQ, "QLCM", "MCLQ");
        cell.mccv = CopySubchunk(bytes, mcnk, offsMCCV, "VCCM", "MCCV");

        const std::size_t totalRefs = static_cast<std::size_t>(cell.header.nDoodadRefs) +
                                      static_cast<std::size_t>(cell.header.nMapObjRefs);
        if (totalRefs > 0)
        {
            if (offsMCRF == 0)
                throw std::runtime_error("MCNK has placement counts but no MCRF offset");
            const std::vector<std::uint8_t> mcrf = CopySubchunk(bytes, mcnk, offsMCRF, "FRCM", "MCRF");
            if (mcrf.size() != 8u + totalRefs * 4u)
                throw std::runtime_error("MCRF payload size disagrees with MCNK placement counts");
            cell.m2Refs.reserve(cell.header.nDoodadRefs);
            cell.wmoRefs.reserve(cell.header.nMapObjRefs);
            for (std::size_t i = 0; i < cell.header.nDoodadRefs; ++i)
                cell.m2Refs.push_back(ReadU32(mcrf, 8u + i * 4u, "MCRF M2 reference"));
            for (std::size_t i = 0; i < cell.header.nMapObjRefs; ++i)
                cell.wmoRefs.push_back(ReadU32(mcrf, 8u + (static_cast<std::size_t>(cell.header.nDoodadRefs) + i) * 4u,
                                               "MCRF WMO reference"));
        }
    }

    if (std::any_of(occupied.begin(), occupied.end(), [](bool present) { return !present; }))
        throw std::runtime_error("ADT does not contain all 256 MCNK coordinates");

    return result;
}

} // namespace turtle335::adt
