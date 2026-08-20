#include "turtle335/adt/WdtWriter.h"
#include "turtle335/adt/WotlkWdtReader.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace turtle335::adt;

static void WriteLe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

int main()
{
    WdtWriterInput sourceInput;
    sourceInput.mphd[0] = 0x04u; // build12340 big-alpha hint
    SetWdtTerrainTile(sourceInput, 2, 3, true);
    sourceInput.tiles[WdtTileSlot(2, 3)].asyncId = 77;
    std::vector<std::uint8_t> bytes = SerializeTerrainWdt(sourceInput);

    // Source reader is intentionally tolerant of additional root chunks.
    const std::size_t extra = bytes.size();
    bytes.resize(extra + 12u, 0);
    std::memcpy(bytes.data() + extra, "TSET", 4);
    WriteLe32(bytes, extra + 4u, 4u);
    WriteLe32(bytes, extra + 8u, 0x12345678u);

    const WotlkWdtDocument source = ParseWotlkWdt(bytes);
    assert(source.version == 18u);
    assert(source.bigAlpha);
    assert(!source.globalWmo);
    assert((source.tiles[WdtTileSlot(2, 3)].flags & 1u) != 0);
    assert(source.tiles[WdtTileSlot(2, 3)].asyncId == 77u);

    const WdtWriterInput target = NormalizeWotlkTerrainWdt(source);
    assert(target.mphd[0] == 0u); // big-alpha is not a target WDT requirement
    assert((target.tiles[WdtTileSlot(2, 3)].flags & 1u) != 0);
    assert(target.tiles[WdtTileSlot(2, 3)].asyncId == 0u);
    const auto targetBytes = SerializeTerrainWdt(target);
    const auto report = ValidateTerrainWdt(targetBytes);
    assert(report.presentTiles == 1u);

    auto globalBytes = SerializeTerrainWdt(WdtWriterInput{});
    WriteLe32(globalBytes, 20u, 1u); // MPHD[0] global-WMO flag
    const WotlkWdtDocument global = ParseWotlkWdt(globalBytes);
    assert(global.globalWmo);
    bool rejected = false;
    try { (void)NormalizeWotlkTerrainWdt(global); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    std::cout << "turtle335_wotlk_wdt_reader_tests: OK\n";
    return 0;
}
