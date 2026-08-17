#include "turtle335/adt/AdtWriter.h"
#include "turtle335/adt/TerrainWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

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
    AdtWriterInput adt;
    adt.textures = {"Tileset\\Elwynn\\ElwynnGrassBase.blp"};

    TerrainCellInput terrainInput;
    terrainInput.heights.fill(50.0f);
    terrainInput.normals.fill(TerrainNormal{0.0f, 1.0f, 0.0f});
    TerrainLayerInput baseLayer;
    baseLayer.textureId = 0;
    terrainInput.layers.push_back(baseLayer);
    const SerializedTerrain terrain = SerializeLegacyTerrain(terrainInput, adt.textures.size());

    constexpr float chunkSize = 100.0f / 3.0f;
    for (std::uint32_t y = 0; y < 16; ++y)
    {
        for (std::uint32_t x = 0; x < 16; ++x)
        {
            // Input storage order is irrelevant; ix/iy determine canonical MCIN slot.
            AdtCellInput& cell = adt.cells[y * 16u + x];
            cell.header.ix = x;
            cell.header.iy = y;
            cell.header.x = -static_cast<float>(x) * chunkSize; // tile 32,32 canonical world basis
            cell.header.z = -static_cast<float>(y) * chunkSize;
            cell.header.areaId = 0;
            ApplyTerrainToMcnk(terrain, cell.header, cell.subchunks);
        }
    }

    const SerializedAdt out = SerializeVanillaAdt(adt);
    ValidateVanillaAdtRoot(out.bytes);

    assert(out.layout.mcinOffset == 84);
    assert(out.layout.mtexOffset != 0);
    assert(out.layout.mmdxOffset == 0);
    assert(out.layout.mddfOffset == 0);

    const std::size_t mcinPayload = 84 + 8;
    for (std::size_t slot = 0; slot < 256; ++slot)
    {
        const std::uint32_t mcnkOffset = ReadLe32(out.bytes.data() + mcinPayload + slot * 16u);
        const std::uint32_t mcnkSize = ReadLe32(out.bytes.data() + mcinPayload + slot * 16u + 4u);
        assert(mcnkOffset == out.layout.mcnkOffsets[slot]);
        assert(mcnkSize == out.layout.mcnkSizes[slot]);
        assert(std::memcmp(out.bytes.data() + mcnkOffset, "KNCM", 4) == 0);

        const std::uint32_t flags = ReadLe32(out.bytes.data() + mcnkOffset + 8u);
        const std::uint32_t ix = ReadLe32(out.bytes.data() + mcnkOffset + 12u);
        const std::uint32_t iy = ReadLe32(out.bytes.data() + mcnkOffset + 16u);
        assert((flags & 0x3Cu) == 0);       // dry cell: no legacy liquid category bits
        assert((flags & (1u << 15)) == 0); // canonical old-MCLQ target path
        assert(ix == slot % 16u);
        assert(iy == slot / 16u);
        assert(ReadLe32(out.bytes.data() + mcnkOffset + 20u) == 1); // nLayers
        assert(ReadLe32(out.bytes.data() + mcnkOffset + 24u) == 0); // nDoodadRefs
        assert(ReadLe32(out.bytes.data() + mcnkOffset + 64u) == 0); // nMapObjRefs

        const std::uint32_t ofsMcvt = ReadLe32(out.bytes.data() + mcnkOffset + 28u);
        const std::uint32_t ofsMcnr = ReadLe32(out.bytes.data() + mcnkOffset + 32u);
        const std::uint32_t ofsMcly = ReadLe32(out.bytes.data() + mcnkOffset + 36u);
        const std::uint32_t ofsMcrf = ReadLe32(out.bytes.data() + mcnkOffset + 40u);
        const std::uint32_t ofsMcal = ReadLe32(out.bytes.data() + mcnkOffset + 44u);
        const std::uint32_t sizeMcal = ReadLe32(out.bytes.data() + mcnkOffset + 48u);
        const std::uint32_t ofsMcse = ReadLe32(out.bytes.data() + mcnkOffset + 96u);
        const std::uint32_t nSndEmitters = ReadLe32(out.bytes.data() + mcnkOffset + 100u);
        const std::uint32_t ofsMclq = ReadLe32(out.bytes.data() + mcnkOffset + 104u);
        const std::uint32_t sizeMclq = ReadLe32(out.bytes.data() + mcnkOffset + 108u);

        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcvt, "TVCM", 4) == 0);
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcnr, "RNCM", 4) == 0);
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcly, "YLCM", 4) == 0);
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcrf, "FRCM", 4) == 0);
        assert(ReadLe32(out.bytes.data() + mcnkOffset + ofsMcrf + 4u) == 0);
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcal, "LACM", 4) == 0);
        assert(sizeMcal == 8); // empty MCAL chunk for the base-only fixture
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMcse, "ESCM", 4) == 0);
        assert(ReadLe32(out.bytes.data() + mcnkOffset + ofsMcse + 4u) == 0);
        assert(nSndEmitters == 0);
        assert(std::memcmp(out.bytes.data() + mcnkOffset + ofsMclq, "QLCM", 4) == 0);
        assert(ReadLe32(out.bytes.data() + mcnkOffset + ofsMclq + 4u) == 0);
        assert(sizeMclq == 8);
    }

    std::cout << "turtle335_fixture_a_tests: OK bytes=" << out.bytes.size() << "\n";
}
