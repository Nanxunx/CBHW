#include "turtle335/adt/PlacementWriter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace turtle335::adt {
namespace {

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
        throw std::invalid_argument("ADT placement contains non-finite float");
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, offset, bits);
}

void WriteVec3(std::vector<std::uint8_t>& out, std::size_t offset, const AdtVec3& value)
{
    WriteF32(out, offset + 0, value.x);
    WriteF32(out, offset + 4, value.y);
    WriteF32(out, offset + 8, value.z);
}

std::uint32_t CheckedU32(std::size_t value, const char* what)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error(std::string(what) + " exceeds uint32 range");
    return static_cast<std::uint32_t>(value);
}

std::vector<std::uint8_t> MakeChunk(const char rawId[4], const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> out(8 + payload.size(), 0);
    std::memcpy(out.data(), rawId, 4);
    WriteU32(out, 4, CheckedU32(payload.size(), "ADT chunk payload"));
    std::copy(payload.begin(), payload.end(), out.begin() + 8);
    return out;
}

std::string NormalizeClientPath(const std::string& input)
{
    if (input.empty())
        throw std::invalid_argument("ADT placement asset path cannot be empty");
    if (input.find('\0') != std::string::npos)
        throw std::invalid_argument("ADT placement asset path contains embedded NUL");

    std::string out = input;
    for (char& ch : out)
    {
        if (ch == '/')
            ch = '\\';
    }
    return out;
}

std::string FoldAscii(const std::string& input)
{
    std::string out = input;
    for (char& ch : out)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

class PathCatalogBuilder
{
public:
    std::uint32_t Resolve(const std::string& sourcePath)
    {
        const std::string path = NormalizeClientPath(sourcePath);
        const std::string key = FoldAscii(path);
        const auto found = nameIds_.find(key);
        if (found != nameIds_.end())
            return found->second;

        const std::uint32_t nameId = CheckedU32(offsets_.size(), "ADT path NameId");
        const std::uint32_t stringOffset = CheckedU32(strings_.size(), "ADT path string offset");
        offsets_.push_back(stringOffset);
        strings_.insert(strings_.end(), path.begin(), path.end());
        strings_.push_back(0);
        nameIds_.emplace(key, nameId);
        return nameId;
    }

    std::vector<std::uint8_t> StringChunk(const char rawId[4]) const
    {
        return MakeChunk(rawId, strings_);
    }

    std::vector<std::uint8_t> IndexChunk(const char rawId[4]) const
    {
        std::vector<std::uint8_t> payload(offsets_.size() * 4u, 0);
        for (std::size_t i = 0; i < offsets_.size(); ++i)
            WriteU32(payload, i * 4u, offsets_[i]);
        return MakeChunk(rawId, payload);
    }

private:
    std::vector<std::uint8_t> strings_;
    std::vector<std::uint32_t> offsets_;
    std::unordered_map<std::string, std::uint32_t> nameIds_;
};

void ValidateWmoBounds(const WmoPlacementInput& placement)
{
    const auto finite = [](const AdtVec3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite(placement.minimumExtent) || !finite(placement.maximumExtent))
        throw std::invalid_argument("WMO placement bounds contain non-finite float");
    if (placement.minimumExtent.x > placement.maximumExtent.x ||
        placement.minimumExtent.y > placement.maximumExtent.y ||
        placement.minimumExtent.z > placement.maximumExtent.z)
        throw std::invalid_argument("WMO placement bounds are reversed");
}

} // namespace

PlacementTables BuildPlacementTables(const std::vector<M2PlacementInput>& m2Placements,
                                     const std::vector<WmoPlacementInput>& wmoPlacements)
{
    PlacementTables result;

    // MDDF and MODF share one map-object UniqueId namespace inside an ADT.
    // Preserve source UIDs, but reject zero or duplicate/cross-family collisions.
    std::unordered_set<std::uint32_t> uniqueIds;
    for (const M2PlacementInput& placement : m2Placements)
    {
        if (placement.uniqueId == 0)
            throw std::invalid_argument("M2 placement UniqueId zero is reserved");
        if (!uniqueIds.insert(placement.uniqueId).second)
            throw std::invalid_argument("duplicate ADT placement UniqueId");
    }
    for (const WmoPlacementInput& placement : wmoPlacements)
    {
        if (placement.uniqueId == 0)
            throw std::invalid_argument("WMO placement UniqueId zero is reserved");
        if (!uniqueIds.insert(placement.uniqueId).second)
            throw std::invalid_argument("duplicate ADT placement UniqueId");
    }

    PathCatalogBuilder m2Paths;
    result.m2NameIds.reserve(m2Placements.size());
    std::vector<std::uint8_t> mddfPayload(m2Placements.size() * 36u, 0);
    for (std::size_t i = 0; i < m2Placements.size(); ++i)
    {
        const M2PlacementInput& placement = m2Placements[i];
        if (placement.scale == 0)
            throw std::invalid_argument("M2 placement scale cannot be zero");

        const std::uint32_t nameId = m2Paths.Resolve(placement.assetPath);
        result.m2NameIds.push_back(nameId);
        const std::size_t at = i * 36u;
        WriteU32(mddfPayload, at + 0, nameId);
        WriteU32(mddfPayload, at + 4, placement.uniqueId);
        WriteVec3(mddfPayload, at + 8, placement.position);
        WriteVec3(mddfPayload, at + 20, placement.rotation);
        WriteU16(mddfPayload, at + 32, placement.scale);
        WriteU16(mddfPayload, at + 34, placement.flags);
    }
    if (!m2Placements.empty())
    {
        result.mmdx = m2Paths.StringChunk("XDMM");
        result.mmid = m2Paths.IndexChunk("DIMM");
        result.mddf = MakeChunk("FDDM", mddfPayload);
    }

    PathCatalogBuilder wmoPaths;
    result.wmoNameIds.reserve(wmoPlacements.size());
    std::vector<std::uint8_t> modfPayload(wmoPlacements.size() * 64u, 0);
    for (std::size_t i = 0; i < wmoPlacements.size(); ++i)
    {
        const WmoPlacementInput& placement = wmoPlacements[i];
        ValidateWmoBounds(placement);

        const std::uint32_t nameId = wmoPaths.Resolve(placement.assetPath);
        result.wmoNameIds.push_back(nameId);
        const std::size_t at = i * 64u;
        WriteU32(modfPayload, at + 0, nameId);
        WriteU32(modfPayload, at + 4, placement.uniqueId);
        WriteVec3(modfPayload, at + 8, placement.position);
        WriteVec3(modfPayload, at + 20, placement.rotation);
        WriteVec3(modfPayload, at + 32, placement.minimumExtent);
        WriteVec3(modfPayload, at + 44, placement.maximumExtent);
        WriteU16(modfPayload, at + 56, placement.flags);
        WriteU16(modfPayload, at + 58, placement.doodadSet);
        WriteU16(modfPayload, at + 60, placement.nameSet);
        WriteU16(modfPayload, at + 62, placement.scale);
    }
    if (!wmoPlacements.empty())
    {
        result.mwmo = wmoPaths.StringChunk("OMWM");
        result.mwid = wmoPaths.IndexChunk("DIWM");
        result.modf = MakeChunk("FDOM", modfPayload);
    }

    return result;
}

std::vector<std::uint8_t> BuildMcrfChunk(const std::vector<std::uint32_t>& m2Refs,
                                         const std::vector<std::uint32_t>& wmoRefs,
                                         std::size_t m2PlacementCount,
                                         std::size_t wmoPlacementCount)
{
    for (std::uint32_t ref : m2Refs)
    {
        if (ref >= m2PlacementCount)
            throw std::out_of_range("MCNK M2 reference exceeds MDDF placement count");
    }
    for (std::uint32_t ref : wmoRefs)
    {
        if (ref >= wmoPlacementCount)
            throw std::out_of_range("MCNK WMO reference exceeds MODF placement count");
    }

    if (m2Refs.empty() && wmoRefs.empty())
        return {};

    const std::size_t count = m2Refs.size() + wmoRefs.size();
    std::vector<std::uint8_t> payload(count * 4u, 0);
    std::size_t cursor = 0;
    for (std::uint32_t ref : m2Refs)
    {
        WriteU32(payload, cursor, ref);
        cursor += 4;
    }
    for (std::uint32_t ref : wmoRefs)
    {
        WriteU32(payload, cursor, ref);
        cursor += 4;
    }
    return MakeChunk("FRCM", payload);
}

} // namespace turtle335::adt
