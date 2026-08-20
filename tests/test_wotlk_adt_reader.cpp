#include "turtle335/adt/AdtWriter.h"
#include "turtle335/adt/WotlkAdtReader.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace turtle335::adt;

int main()
{
    auto input = std::make_unique<AdtWriterInput>();
    input->textures = {"Tileset\\Test\\Base.blp", "Tileset\\Test\\Detail.blp"};

    M2PlacementInput m2;
    m2.assetPath = "World\\Generic\\Test\\ReaderModel.m2";
    m2.uniqueId = 1234;
    m2.position = {1.0f, 2.0f, 3.0f};
    m2.rotation = {4.0f, 5.0f, 6.0f};
    m2.scale = 1536;
    m2.flags = 7;
    input->m2Placements.push_back(m2);

    WmoPlacementInput wmo;
    wmo.assetPath = "World\\Wmo\\Test\\ReaderHouse.wmo";
    wmo.uniqueId = 5678;
    wmo.position = {10.0f, 20.0f, 30.0f};
    wmo.rotation = {1.0f, 2.0f, 3.0f};
    wmo.minimumExtent = {-4.0f, -5.0f, -6.0f};
    wmo.maximumExtent = {4.0f, 5.0f, 6.0f};
    wmo.flags = 9;
    wmo.doodadSet = 2;
    wmo.nameSet = 3;
    wmo.scale = 1024;
    input->wmoPlacements.push_back(wmo);

    for (std::uint32_t y = 0; y < 16; ++y)
    {
        for (std::uint32_t x = 0; x < 16; ++x)
        {
            AdtCellInput& cell = input->cells[y * 16u + x];
            cell.header.ix = x;
            cell.header.iy = y;
            cell.header.areaId = 42;
            cell.header.x = static_cast<float>(x);
            cell.header.z = static_cast<float>(y);
            cell.header.y = 100.0f;
        }
    }

    input->cells[0].header.holes = 0x4321;
    input->cells[0].header.legacy3E = 0xABCD;
    input->cells[0].m2Refs = {0};
    input->cells[0].wmoRefs = {0};

    const SerializedAdt serialized = SerializeVanillaAdt(*input);
    const WotlkAdtDocument parsed = ParseWotlkAdt(serialized.bytes);

    assert(parsed.version == 18);
    assert(parsed.textures.size() == 2);
    assert(parsed.textures[0] == "Tileset\\Test\\Base.blp");
    assert(parsed.m2Placements.size() == 1);
    assert(parsed.wmoPlacements.size() == 1);

    assert(parsed.m2Placements[0].assetPath == m2.assetPath);
    assert(parsed.m2Placements[0].uniqueId == m2.uniqueId);
    assert(parsed.m2Placements[0].scale == m2.scale);
    assert(parsed.m2Placements[0].flags == m2.flags);

    assert(parsed.wmoPlacements[0].assetPath == wmo.assetPath);
    assert(parsed.wmoPlacements[0].uniqueId == wmo.uniqueId);
    assert(parsed.wmoPlacements[0].doodadSet == wmo.doodadSet);
    assert(parsed.wmoPlacements[0].nameSet == wmo.nameSet);

    assert(parsed.cells[0].header.ix == 0);
    assert(parsed.cells[0].header.iy == 0);
    assert(parsed.cells[0].header.holes == 0x4321);
    assert(parsed.cells[0].header.legacy3E == 0xABCD);
    assert(parsed.cells[0].m2Refs.size() == 1 && parsed.cells[0].m2Refs[0] == 0);
    assert(parsed.cells[0].wmoRefs.size() == 1 && parsed.cells[0].wmoRefs[0] == 0);
    assert(parsed.cells[0].mclq.size() == 8); // canonical dry QLCM from target writer
    assert(parsed.cells[255].header.ix == 15);
    assert(parsed.cells[255].header.iy == 15);

    // Structural parser must reject a missing MCNK rather than returning a
    // partial document that could later be serialized as a plausible target.
    auto bad = serialized.bytes;
    constexpr std::size_t firstMcinEntry = 84u + 8u;
    bad[firstMcinEntry + 0] = 0;
    bad[firstMcinEntry + 1] = 0;
    bad[firstMcinEntry + 2] = 0;
    bad[firstMcinEntry + 3] = 0;
    bool rejected = false;
    try { (void)ParseWotlkAdt(bad); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    std::cout << "turtle335_wotlk_adt_reader_tests: OK\n";
    return 0;
}
