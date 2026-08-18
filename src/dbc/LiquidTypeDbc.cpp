#include "turtle335/dbc/LiquidTypeDbc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace turtle335::dbc {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string("LiquidType.dbc truncated at ") + what);
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

adt::LiquidCategory CategoryFromSoundBank(std::uint32_t soundBank) noexcept
{
    switch (soundBank)
    {
        case 0: return adt::LiquidCategory::Water;
        case 1: return adt::LiquidCategory::Ocean;
        case 2: return adt::LiquidCategory::Magma;
        case 3: return adt::LiquidCategory::Slime;
        default: return adt::LiquidCategory::Unknown;
    }
}

} // namespace

adt::LiquidCategory LiquidTypeDbc::Resolve(std::uint16_t sourceLiquidType) const noexcept
{
    const auto found = records_.find(sourceLiquidType);
    return found == records_.end() ? adt::LiquidCategory::Unknown : found->second.category;
}

LiquidTypeDbc ParseLiquidTypeDbc(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < 20u)
        throw std::runtime_error("LiquidType.dbc is shorter than a WDBC header");
    if (std::memcmp(bytes.data(), "WDBC", 4) != 0)
        throw std::runtime_error("LiquidType.dbc is not WDBC");

    const std::uint32_t recordCount = ReadU32(bytes, 4u, "record count");
    const std::uint32_t fieldCount = ReadU32(bytes, 8u, "field count");
    const std::uint32_t recordSize = ReadU32(bytes, 12u, "record size");
    const std::uint32_t stringBlockSize = ReadU32(bytes, 16u, "string block size");

    if (fieldCount < 4u)
        throw std::runtime_error("LiquidType.dbc has fewer than four fields");
    if (fieldCount > std::numeric_limits<std::uint32_t>::max() / 4u)
        throw std::runtime_error("LiquidType.dbc field count overflows a WDBC record size");
    if (recordSize != fieldCount * 4u)
        throw std::runtime_error("LiquidType.dbc WDBC record size does not equal fieldCount*4");

    const std::uint64_t recordsBytes = static_cast<std::uint64_t>(recordCount) * recordSize;
    const std::uint64_t expected = 20ull + recordsBytes + stringBlockSize;
    if (expected != bytes.size())
        throw std::runtime_error("LiquidType.dbc WDBC header sizes do not match file length");
    if (recordsBytes > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("LiquidType.dbc record table is too large for this process");

    LiquidTypeDbc result;
    result.records_.reserve(recordCount);
    for (std::uint32_t index = 0; index < recordCount; ++index)
    {
        const std::size_t at = 20u + static_cast<std::size_t>(index) * recordSize;
        LiquidTypeRecord record;
        record.id = ReadU32(bytes, at + 0u, "record ID");
        record.soundBank = ReadU32(bytes, at + 12u, "SoundBank");
        record.category = CategoryFromSoundBank(record.soundBank);
        if (!result.records_.emplace(record.id, record).second)
            throw std::runtime_error("LiquidType.dbc contains duplicate IDs");
    }
    return result;
}

adt::LiquidTypeResolver MakeLiquidTypeResolver(LiquidTypeDbc table)
{
    return [table = std::move(table)](std::uint16_t sourceLiquidType) {
        return table.Resolve(sourceLiquidType);
    };
}

} // namespace turtle335::dbc
