#include "turtle335/adt/AdtWriter.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::adt {
namespace {

constexpr std::uint32_t kAdtVersion = 18;
constexpr std::size_t kMverSize = 12;
constexpr std::size_t kMhdrChunkSize = 72;
constexpr std::size_t kMcinPayloadSize = 256u * 16u;
constexpr std::size_t kMcinChunkSize = 8u + kMcinPayloadSize;
constexpr std::size_t kCanonicalMcinOffset = kMverSize + kMhdrChunkSize; // 84

void WriteU32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value)
{
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

std::uint32_t ReadU32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t CheckedU32(std::size_t value, const char* what)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error(std::string(what) + " exceeds uint32 range");
    return static_cast<std::uint32_t>(value);
}

std::vector<std::uint8_t> MakeChunk(const char rawId[4], const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> out(8u + payload.size(), 0);
    std::memcpy(out.data(), rawId, 4);
    WriteU32(out, 4, CheckedU32(payload.size(), "ADT chunk payload"));
    std::copy(payload.begin(), payload.end(), out.begin() + 8);
    return out;
}

std::vector<std::uint8_t> MakeMver()
{
    std::vector<std::uint8_t> payload(4, 0);
    WriteU32(payload, 0, kAdtVersion);
    return MakeChunk("REVM", payload);
}

std::vector<std::uint8_t> MakeMhdr()
{
    return MakeChunk("RDHM", std::vector<std::uint8_t>(64, 0));
}

std::vector<std::uint8_t> MakeMcin()
{
    return MakeChunk("NICM", std::vector<std::uint8_t>(kMcinPayloadSize, 0));
}

std::vector<std::uint8_t> MakeStringChunk(const char rawId[4], const std::vector<std::string>& strings)
{
    if (strings.empty())
        return {};

    std::vector<std::uint8_t> payload;
    for (const std::string& source : strings)
    {
        if (source.empty())
            throw std::invalid_argument("ADT texture path cannot be empty");
        if (source.find('\0') != std::string::npos)
            throw std::invalid_argument("ADT texture path contains embedded NUL");
        std::string path = source;
        for (char& ch : path)
        {
            if (ch == '/')
                ch = '\\';
        }
        payload.insert(payload.end(), path.begin(), path.end());
        payload.push_back(0);
    }
    return MakeChunk(rawId, payload);
}

void ValidateRawChunk(const std::vector<std::uint8_t>& chunk, const char rawId[4], const char* name)
{
    if (chunk.empty())
        return;
    if (chunk.size() < 8 || std::memcmp(chunk.data(), rawId, 4) != 0)
        throw std::invalid_argument(std::string(name) + " raw chunk header mismatch");
    if (static_cast<std::size_t>(ReadU32(chunk.data() + 4)) + 8u != chunk.size())
        throw std::invalid_argument(std::string(name) + " declared size does not match bytes");
}

std::uint32_t Append(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& chunk)
{
    if (chunk.empty())
        return 0;
    const std::uint32_t offset = CheckedU32(out.size(), "ADT top-level chunk offset");
    out.insert(out.end(), chunk.begin(), chunk.end());
    return offset;
}

std::size_t Slot(std::uint32_t x, std::uint32_t y)
{
    if (x >= 16 || y >= 16)
        throw std::invalid_argument("ADT MCNK ix/iy must be within 0..15");
    // Blizzard/Noggit MCIN order is row-major by iy, then ix: py*16+px.
    // Tortoise's ConvertADT likewise treats getMCNK(i,j) as y,x.
    return static_cast<std::size_t>(y) * 16u + static_cast<std::size_t>(x);
}

void PatchMhdrOffset(std::vector<std::uint8_t>& bytes,
                     std::uint32_t mhdrOffset,
                     std::size_t payloadFieldOffset,
                     std::uint32_t targetOffset)
{
    const std::uint32_t base = mhdrOffset + 8u;
    if (targetOffset != 0 && targetOffset < base)
        throw std::logic_error("MHDR target precedes MHDR payload base");
    WriteU32(bytes, static_cast<std::size_t>(base) + payloadFieldOffset,
             targetOffset == 0 ? 0u : targetOffset - base);
}

void RequireRange(std::size_t offset, std::size_t size, std::size_t total, const char* what)
{
    if (offset > total || size > total - offset)
        throw std::runtime_error(std::string(what) + " is outside ADT bytes");
}

void RequireRawId(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char id[4], const char* what)
{
    RequireRange(offset, 4, bytes.size(), what);
    if (std::memcmp(bytes.data() + offset, id, 4) != 0)
        throw std::runtime_error(std::string(what) + " raw FourCC mismatch");
}

} // namespace

SerializedAdt SerializeVanillaAdt(const AdtWriterInput& input)
{
    ValidateRawChunk(input.mfbo, "OBFM", "MFBO");

    SerializedAdt result;
    result.placements = BuildPlacementTables(input.m2Placements, input.wmoPlacements);

    std::array<const AdtCellInput*, 256> bySlot{};
    for (const AdtCellInput& cell : input.cells)
    {
        const std::size_t slot = Slot(cell.header.ix, cell.header.iy);
        if (bySlot[slot] != nullptr)
            throw std::invalid_argument("ADT contains duplicate MCNK ix/iy");
        if (!cell.subchunks.mcrf.empty())
            throw std::invalid_argument("Full ADT writer owns MCRF; caller-supplied raw MCRF must be empty");
        bySlot[slot] = &cell;
    }
    if (std::any_of(bySlot.begin(), bySlot.end(), [](const AdtCellInput* cell) { return cell == nullptr; }))
        throw std::invalid_argument("ADT must contain exactly one MCNK for every ix/iy pair");

    std::array<SerializedMcnk, 256> mcnks;
    for (std::size_t slot = 0; slot < bySlot.size(); ++slot)
    {
        const AdtCellInput& source = *bySlot[slot];
        McnkTargetHeader header = source.header;
        McnkSubchunks subchunks = source.subchunks;
        subchunks.mcrf = BuildMcrfChunk(source.m2Refs, source.wmoRefs,
                                        input.m2Placements.size(), input.wmoPlacements.size());
        header.nDoodadRefs = CheckedU32(source.m2Refs.size(), "MCNK M2 reference count");
        header.nMapObjRefs = CheckedU32(source.wmoRefs.size(), "MCNK WMO reference count");
        mcnks[slot] = SerializeVanillaMcnk(header, subchunks);
        if (mcnks[slot].bytes.size() < 8 || std::memcmp(mcnks[slot].bytes.data(), "KNCM", 4) != 0)
            throw std::runtime_error("SerializeVanillaMcnk returned invalid raw MCNK");
    }

    const std::vector<std::uint8_t> mver = MakeMver();
    const std::vector<std::uint8_t> mhdr = MakeMhdr();
    const std::vector<std::uint8_t> mcin = MakeMcin();
    const std::vector<std::uint8_t> mtex = MakeStringChunk("XETM", input.textures);

    result.layout.mverOffset = Append(result.bytes, mver);
    result.layout.mhdrOffset = Append(result.bytes, mhdr);
    result.layout.mcinOffset = Append(result.bytes, mcin);
    if (result.layout.mcinOffset != kCanonicalMcinOffset)
        throw std::logic_error("Canonical ADT root no longer places MCIN at byte 84");

    result.layout.mtexOffset = Append(result.bytes, mtex);
    result.layout.mmdxOffset = Append(result.bytes, result.placements.mmdx);
    result.layout.mmidOffset = Append(result.bytes, result.placements.mmid);
    result.layout.mwmoOffset = Append(result.bytes, result.placements.mwmo);
    result.layout.mwidOffset = Append(result.bytes, result.placements.mwid);
    result.layout.mddfOffset = Append(result.bytes, result.placements.mddf);
    result.layout.modfOffset = Append(result.bytes, result.placements.modf);
    result.layout.mfboOffset = Append(result.bytes, input.mfbo);

    for (std::size_t slot = 0; slot < mcnks.size(); ++slot)
    {
        result.layout.mcnkOffsets[slot] = CheckedU32(result.bytes.size(), "MCNK absolute file offset");
        result.layout.mcnkSizes[slot] = CheckedU32(mcnks[slot].bytes.size(), "MCNK byte size");
        result.bytes.insert(result.bytes.end(), mcnks[slot].bytes.begin(), mcnks[slot].bytes.end());
    }

    // MHDR payload starts at chunk+8. Offset fields begin after the 4-byte pad.
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 4, result.layout.mcinOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 8, result.layout.mtexOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 12, result.layout.mmdxOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 16, result.layout.mmidOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 20, result.layout.mwmoOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 24, result.layout.mwidOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 28, result.layout.mddfOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 32, result.layout.modfOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 36, result.layout.mfboOffset);
    PatchMhdrOffset(result.bytes, result.layout.mhdrOffset, 40, 0); // MH2O intentionally absent in target client ADT.

    // MCIN entries are y-major: slot = iy*16+ix.
    const std::size_t mcinPayload = static_cast<std::size_t>(result.layout.mcinOffset) + 8u;
    for (std::size_t slot = 0; slot < 256; ++slot)
    {
        const std::size_t at = mcinPayload + slot * 16u;
        WriteU32(result.bytes, at + 0, result.layout.mcnkOffsets[slot]);
        WriteU32(result.bytes, at + 4, result.layout.mcnkSizes[slot]);
        WriteU32(result.bytes, at + 8, 0);
        WriteU32(result.bytes, at + 12, 0);
    }

    ValidateVanillaAdtRoot(result.bytes);
    return result;
}

void ValidateVanillaAdtRoot(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < kCanonicalMcinOffset + kMcinChunkSize)
        throw std::runtime_error("ADT is too small for canonical MVER/MHDR/MCIN root");

    RequireRawId(bytes, 0, "REVM", "MVER");
    if (ReadU32(bytes.data() + 4) != 4 || ReadU32(bytes.data() + 8) != kAdtVersion)
        throw std::runtime_error("ADT MVER must be version 18 with 4-byte payload");

    RequireRawId(bytes, kMverSize, "RDHM", "MHDR");
    if (ReadU32(bytes.data() + kMverSize + 4) != 64)
        throw std::runtime_error("ADT MHDR payload must be 64 bytes");

    RequireRawId(bytes, kCanonicalMcinOffset, "NICM", "MCIN");
    if (ReadU32(bytes.data() + kCanonicalMcinOffset + 4) != kMcinPayloadSize)
        throw std::runtime_error("ADT MCIN payload must be 4096 bytes");

    const std::size_t mhdrPayload = kMverSize + 8u;
    const std::uint32_t mcinRel = ReadU32(bytes.data() + mhdrPayload + 4u);
    if (mhdrPayload + mcinRel != kCanonicalMcinOffset)
        throw std::runtime_error("ADT MHDR offsMCIN does not resolve to byte 84");

    const auto validateMhdrTarget = [&](std::size_t fieldOffset, const char rawId[4], const char* name) {
        const std::uint32_t rel = ReadU32(bytes.data() + mhdrPayload + fieldOffset);
        if (rel == 0)
            return;
        const std::size_t absolute = mhdrPayload + rel;
        RequireRawId(bytes, absolute, rawId, name);
        RequireRange(absolute, 8u + ReadU32(bytes.data() + absolute + 4), bytes.size(), name);
    };
    validateMhdrTarget(8,  "XETM", "MTEX");
    validateMhdrTarget(12, "XDMM", "MMDX");
    validateMhdrTarget(16, "DIMM", "MMID");
    validateMhdrTarget(20, "OMWM", "MWMO");
    validateMhdrTarget(24, "DIWM", "MWID");
    validateMhdrTarget(28, "FDDM", "MDDF");
    validateMhdrTarget(32, "FDOM", "MODF");
    validateMhdrTarget(36, "OBFM", "MFBO");
    if (ReadU32(bytes.data() + mhdrPayload + 40u) != 0)
        throw std::runtime_error("canonical Turtle target ADT must not retain MH2O");

    const std::size_t mcinPayload = kCanonicalMcinOffset + 8u;
    for (std::size_t slot = 0; slot < 256; ++slot)
    {
        const std::size_t at = mcinPayload + slot * 16u;
        const std::uint32_t offset = ReadU32(bytes.data() + at + 0);
        const std::uint32_t size = ReadU32(bytes.data() + at + 4);
        if (offset == 0 || size < 8)
            throw std::runtime_error("ADT MCIN contains missing/short MCNK entry");
        RequireRange(offset, size, bytes.size(), "MCIN MCNK range");
        RequireRawId(bytes, offset, "KNCM", "MCNK");
        const std::uint32_t declaredPayload = ReadU32(bytes.data() + offset + 4);
        if (static_cast<std::uint64_t>(declaredPayload) + 8u != size)
            throw std::runtime_error("MCIN MCNK size disagrees with MCNK chunk header");

        const std::uint32_t expectedX = static_cast<std::uint32_t>(slot % 16u);
        const std::uint32_t expectedY = static_cast<std::uint32_t>(slot / 16u);
        if (ReadU32(bytes.data() + offset + 12u) != expectedX ||
            ReadU32(bytes.data() + offset + 16u) != expectedY)
            throw std::runtime_error("MCIN y-major slot disagrees with MCNK ix/iy");
    }
}

} // namespace turtle335::adt
