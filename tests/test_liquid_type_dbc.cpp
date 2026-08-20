#include "turtle335/dbc/LiquidTypeDbc.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace turtle335;

static void WriteLe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

static std::vector<std::uint8_t> MakeLiquidTypeDbc()
{
    constexpr std::uint32_t count = 4;
    constexpr std::uint32_t fields = 4;
    constexpr std::uint32_t recordSize = fields * 4;
    constexpr std::uint32_t strings = 1;
    std::vector<std::uint8_t> bytes(20u + count * recordSize + strings, 0);
    std::memcpy(bytes.data(), "WDBC", 4);
    WriteLe32(bytes, 4, count);
    WriteLe32(bytes, 8, fields);
    WriteLe32(bytes, 12, recordSize);
    WriteLe32(bytes, 16, strings);

    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::size_t at = 20u + static_cast<std::size_t>(i) * recordSize;
        WriteLe32(bytes, at + 0, 7u + i);   // ID
        WriteLe32(bytes, at + 12, i);       // SoundBank
    }
    return bytes;
}

int main()
{
    const dbc::LiquidTypeDbc table = dbc::ParseLiquidTypeDbc(MakeLiquidTypeDbc());
    assert(table.Records().size() == 4);
    assert(table.Resolve(7) == adt::LiquidCategory::Water);
    assert(table.Resolve(8) == adt::LiquidCategory::Ocean);
    assert(table.Resolve(9) == adt::LiquidCategory::Magma);
    assert(table.Resolve(10) == adt::LiquidCategory::Slime);
    assert(table.Resolve(99) == adt::LiquidCategory::Unknown);

    auto resolver = dbc::MakeLiquidTypeResolver(table);
    assert(resolver(8) == adt::LiquidCategory::Ocean);

    auto duplicate = MakeLiquidTypeDbc();
    WriteLe32(duplicate, 20u + 16u, 7u);
    bool rejected = false;
    try { (void)dbc::ParseLiquidTypeDbc(duplicate); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    std::cout << "turtle335_liquid_type_dbc_tests: OK\n";
    return 0;
}
