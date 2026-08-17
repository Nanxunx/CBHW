#include "turtle335/adt/AdtWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
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
    auto input = std::make_unique<AdtWriterInput>();
    input->textures = {"Tileset/Test/A.blp", "Tileset\\Test\\B.blp"};

    M2PlacementInput m2;
    m2.assetPath = "World/Generic/Test/TestModel.m2";
    m2.uniqueId = 100;
    m2.position = {1.0f, 2.0f, 3.0f};
    m2.rotation = {4.0f, 5.0f, 6.0f};
    input->m2Placements.push_back(m2);
    auto m2DuplicatePath = m2;
    m2DuplicatePath.assetPath = "world\\generic\\test\\testmodel.m2";
    m2DuplicatePath.uniqueId = 101;
    input->m2Placements.push_back(m2DuplicatePath);

    WmoPlacementInput wmo;
    wmo.assetPath = "World/Wmo/Test/Test.wmo";
    wmo.uniqueId = 200;
    wmo.minimumExtent = {-10.0f, -20.0f, -30.0f};
    wmo.maximumExtent = {10.0f, 20.0f, 30.0f};
    input->wmoPlacements.push_back(wmo);

    for (std::uint32_t x = 0; x < 16; ++x)
    {
        for (std::uint32_t y = 0; y < 16; ++y)
        {
            auto& cell = input->cells[x * 16 + y];
            cell.header.ix = x;
            cell.header.iy = y;
        }
    }
    input->cells[0].m2Refs = {0, 1};
    input->cells[0].wmoRefs = {0};

    const auto out = SerializeVanillaAdt(*input);
    ValidateVanillaAdtRoot(out.bytes);

    assert(out.layout.mverOffset == 0);
    assert(out.layout.mhdrOffset == 12);
    assert(out.layout.mcinOffset == 84);
    assert(std::memcmp(out.bytes.data(), "REVM", 4) == 0);
    assert(ReadLe32(out.bytes.data() + 8) == 18);
    assert(std::memcmp(out.bytes.data() + 12, "RDHM", 4) == 0);
    assert(std::memcmp(out.bytes.data() + 84, "NICM", 4) == 0);

    // MHDR offsets are relative to MHDR payload start (byte 20).
    assert(20u + ReadLe32(out.bytes.data() + 20 + 4) == 84u);
    assert(20u + ReadLe32(out.bytes.data() + 20 + 8) == out.layout.mtexOffset);
    assert(20u + ReadLe32(out.bytes.data() + 20 + 12) == out.layout.mmdxOffset);
    assert(20u + ReadLe32(out.bytes.data() + 20 + 16) == out.layout.mmidOffset);
    assert(ReadLe32(out.bytes.data() + 20 + 40) == 0); // offsMH2O

    // Case/slash-equivalent M2 paths dedupe to one catalog NameId.
    assert(out.placements.m2NameIds.size() == 2);
    assert(out.placements.m2NameIds[0] == 0);
    assert(out.placements.m2NameIds[1] == 0);
    assert(ReadLe32(out.placements.mmid.data() + 4) == 4); // one uint32 offset payload

    // First MCIN slot is cell (ix=0,iy=0). Full writer owns MCRF/counts.
    const std::size_t firstMcin = 84 + 8;
    assert(ReadLe32(out.bytes.data() + firstMcin + 0) == out.layout.mcnkOffsets[0]);
    assert(ReadLe32(out.bytes.data() + firstMcin + 4) == out.layout.mcnkSizes[0]);
    const std::size_t firstMcnk = out.layout.mcnkOffsets[0];
    assert(ReadLe32(out.bytes.data() + firstMcnk + 12) == 0); // ix
    assert(ReadLe32(out.bytes.data() + firstMcnk + 16) == 0); // iy
    assert(ReadLe32(out.bytes.data() + firstMcnk + 24) == 2); // nDoodadRefs
    assert(ReadLe32(out.bytes.data() + firstMcnk + 64) == 1); // nMapObjRefs
    const std::uint32_t offsMcrf = ReadLe32(out.bytes.data() + firstMcnk + 40);
    assert(offsMcrf != 0);
    assert(std::memcmp(out.bytes.data() + firstMcnk + offsMcrf, "FRCM", 4) == 0);
    assert(ReadLe32(out.bytes.data() + firstMcnk + offsMcrf + 4) == 12);
    assert(ReadLe32(out.bytes.data() + firstMcnk + offsMcrf + 8) == 0);
    assert(ReadLe32(out.bytes.data() + firstMcnk + offsMcrf + 12) == 1);
    assert(ReadLe32(out.bytes.data() + firstMcnk + offsMcrf + 16) == 0);

    // MCIN is y-major: slot 1=(x=1,y=0), slot 16=(x=0,y=1).
    const std::size_t slot1 = firstMcin + 1 * 16;
    const std::size_t mcnk1 = ReadLe32(out.bytes.data() + slot1);
    assert(ReadLe32(out.bytes.data() + mcnk1 + 12) == 1);
    assert(ReadLe32(out.bytes.data() + mcnk1 + 16) == 0);

    const std::size_t slot16 = firstMcin + 16 * 16;
    const std::size_t mcnk16 = ReadLe32(out.bytes.data() + slot16);
    assert(ReadLe32(out.bytes.data() + mcnk16 + 12) == 0);
    assert(ReadLe32(out.bytes.data() + mcnk16 + 16) == 1);

    // Corrupt MCIN size must be detected by root validator.
    auto corrupt = out.bytes;
    corrupt[firstMcin + 4] ^= 1;
    bool rejected = false;
    try { ValidateVanillaAdtRoot(corrupt); }
    catch (...) { rejected = true; }
    assert(rejected);

    // Heap copies avoid exceeding Windows' default 1 MiB test stack.
    {
        auto duplicateUid = std::make_unique<AdtWriterInput>(*input);
        duplicateUid->wmoPlacements[0].uniqueId = 100;
        rejected = false;
        try { (void)SerializeVanillaAdt(*duplicateUid); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    // MCRF cannot reference a placement outside MDDF/MODF.
    {
        auto badRef = std::make_unique<AdtWriterInput>(*input);
        badRef->cells[0].m2Refs.push_back(99);
        rejected = false;
        try { (void)SerializeVanillaAdt(*badRef); }
        catch (const std::out_of_range&) { rejected = true; }
        assert(rejected);
    }

    std::cout << "turtle335_full_adt_tests: OK\n";
}
