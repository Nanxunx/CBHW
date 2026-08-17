#include "turtle335/adt/WdtWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace turtle335::adt;

static std::uint32_t ReadLe32(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

int main()
{
    WdtWriterInput input;
    SetWdtTerrainTile(input, 32, 32, true);

    const auto bytes = SerializeTerrainWdt(input);
    const auto report = ValidateTerrainWdt(bytes);

    assert(bytes.size() == 32828);
    assert(report.fileSize == 32828);
    assert(report.version == 18);
    assert(report.headerFlags == 0);
    assert(report.presentTiles == 1);

    assert(std::memcmp(bytes.data(), "REVM", 4) == 0);
    assert(ReadLe32(bytes.data() + 4) == 4);
    assert(ReadLe32(bytes.data() + 8) == 18);
    assert(std::memcmp(bytes.data() + 12, "DHPM", 4) == 0);
    assert(ReadLe32(bytes.data() + 16) == 32);
    assert(std::memcmp(bytes.data() + 52, "NIAM", 4) == 0);
    assert(ReadLe32(bytes.data() + 56) == 64u * 64u * 8u);

    constexpr std::size_t mainPayload = 60;
    const std::size_t slot = WdtTileSlot(32, 32);
    assert(slot == 32u * 64u + 32u);
    assert(ReadLe32(bytes.data() + mainPayload + slot * 8u) == 1);
    assert(ReadLe32(bytes.data() + mainPayload + slot * 8u + 4u) == 0);
    assert(ReadLe32(bytes.data() + mainPayload + WdtTileSlot(31, 32) * 8u) == 0);
    assert(ReadLe32(bytes.data() + mainPayload + WdtTileSlot(32, 31) * 8u) == 0);

    // Presence toggles only MAIN bit0, preserving other reviewed flags/asyncId.
    WdtWriterInput preserved;
    preserved.tiles[WdtTileSlot(2, 3)].flags = 0x20u;
    preserved.tiles[WdtTileSlot(2, 3)].asyncId = 77u;
    SetWdtTerrainTile(preserved, 2, 3, true);
    auto preservedBytes = SerializeTerrainWdt(preserved);
    const std::size_t at = mainPayload + WdtTileSlot(2, 3) * 8u;
    assert(ReadLe32(preservedBytes.data() + at) == 0x21u);
    assert(ReadLe32(preservedBytes.data() + at + 4u) == 77u);
    SetWdtTerrainTile(preserved, 2, 3, false);
    preservedBytes = SerializeTerrainWdt(preserved);
    assert(ReadLe32(preservedBytes.data() + at) == 0x20u);

    bool rejected = false;
    try { SetWdtTerrainTile(input, 64, 0, true); }
    catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);

    WdtWriterInput global;
    global.mphd[0] = 1;
    rejected = false;
    try { (void)SerializeTerrainWdt(global); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    auto corrupt = bytes;
    corrupt[52] = 'X';
    rejected = false;
    try { (void)ValidateTerrainWdt(corrupt); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    std::cout << "turtle335_wdt_tests: OK\n";
}
