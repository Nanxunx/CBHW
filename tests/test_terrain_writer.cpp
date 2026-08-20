#include "turtle335/adt/TerrainWriter.h"

#include <cassert>
#include <cmath>
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

static float ReadLeF32(const std::uint8_t* p)
{
    const std::uint32_t bits = ReadLe32(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static TerrainCellInput FlatCell(std::size_t layers)
{
    TerrainCellInput cell;
    for (std::size_t i = 0; i < cell.heights.size(); ++i)
    {
        cell.heights[i] = 100.0f + static_cast<float>(i) * 0.25f;
        cell.normals[i] = {0.0f, 1.0f, 0.0f};
    }

    for (std::size_t i = 0; i < layers; ++i)
    {
        TerrainLayerInput layer;
        layer.textureId = static_cast<std::uint32_t>(i);
        layer.effectId = 0xFFFFu;
        if (i > 0)
        {
            Alpha8 alpha{};
            alpha.fill(static_cast<std::uint8_t>(i == 1 ? 64 : 128));
            layer.alpha = alpha;
        }
        cell.layers.push_back(layer);
    }
    return cell;
}

int main()
{
    {
        const auto terrain = SerializeLegacyTerrain(FlatCell(1), 1);
        assert(terrain.baseHeight == 100.0f);
        assert(terrain.nLayers == 1);

        assert(std::memcmp(terrain.mcvt.data(), "TVCM", 4) == 0);
        assert(ReadLe32(terrain.mcvt.data() + 4) == 145 * 4);
        assert(ReadLeF32(terrain.mcvt.data() + 8) == 0.0f);
        assert(std::abs(ReadLeF32(terrain.mcvt.data() + 12) - 0.25f) < 0.00001f);

        assert(std::memcmp(terrain.mcnr.data(), "RNCM", 4) == 0);
        assert(ReadLe32(terrain.mcnr.data() + 4) == 145 * 3);
        assert(terrain.mcnr[8] == 0);   // x
        assert(terrain.mcnr[9] == 0);   // z
        assert(terrain.mcnr[10] == 127); // y

        assert(std::memcmp(terrain.mcly.data(), "YLCM", 4) == 0);
        assert(ReadLe32(terrain.mcly.data() + 4) == 16);
        assert(ReadLe32(terrain.mcly.data() + 8 + 0) == 0);
        assert((ReadLe32(terrain.mcly.data() + 8 + 4) & 0x300u) == 0);
        assert(ReadLe32(terrain.mcly.data() + 8 + 8) == 0);

        assert(std::memcmp(terrain.mcal.data(), "LACM", 4) == 0);
        assert(ReadLe32(terrain.mcal.data() + 4) == 0);
        assert(terrain.mcal.size() == 8);

        McnkTargetHeader header;
        McnkSubchunks chunks;
        ApplyTerrainToMcnk(terrain, header, chunks);
        assert(header.nLayers == 1);
        assert(header.y == 100.0f);
        const auto mcnk = SerializeVanillaMcnk(header, chunks);
        assert(mcnk.layout.offsMCLY == mcnk.layout.offsMCNR + terrain.mcnr.size() + 13);
        assert(mcnk.layout.sizeMCAL == 8);
        assert((ReadLe32(mcnk.bytes.data() + 8) & (1u << 15)) == 0);
    }

    {
        const auto terrain = SerializeLegacyTerrain(FlatCell(3), 3);
        assert(terrain.nLayers == 3);
        assert(ReadLe32(terrain.mcly.data() + 4) == 48);
        assert(ReadLe32(terrain.mcal.data() + 4) == 4096);
        assert(terrain.mcal.size() == 4104);

        const std::size_t layer0 = 8;
        const std::size_t layer1 = 8 + 16;
        const std::size_t layer2 = 8 + 32;
        assert((ReadLe32(terrain.mcly.data() + layer0 + 4) & 0x300u) == 0);
        assert((ReadLe32(terrain.mcly.data() + layer1 + 4) & 0x100u) != 0);
        assert((ReadLe32(terrain.mcly.data() + layer1 + 4) & 0x200u) == 0);
        assert((ReadLe32(terrain.mcly.data() + layer2 + 4) & 0x100u) != 0);
        assert(ReadLe32(terrain.mcly.data() + layer1 + 8) == 0);
        assert(ReadLe32(terrain.mcly.data() + layer2 + 8) == 2048);
    }

    {
        auto invalid = FlatCell(3);
        invalid.layers[1].alpha->fill(200);
        invalid.layers[2].alpha->fill(100);
        bool rejected = false;
        try { (void)SerializeLegacyTerrain(invalid, 3); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    {
        auto invalid = FlatCell(2);
        invalid.layers[1].textureId = 9;
        bool rejected = false;
        try { (void)SerializeLegacyTerrain(invalid, 2); }
        catch (const std::out_of_range&) { rejected = true; }
        assert(rejected);
    }

    std::cout << "turtle335_terrain_tests: OK\n";
}
