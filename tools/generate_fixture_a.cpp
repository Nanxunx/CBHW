#include "turtle335/adt/AdtWriter.h"
#include "turtle335/adt/TerrainWriter.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace turtle335::adt;

int main(int argc, char** argv)
{
    try
    {
        const std::string outputPath = argc >= 2 ? argv[1] : "FixtureA_32_32.adt";

        auto adt = std::make_unique<AdtWriterInput>();
        // Widely used legacy Elwynn base terrain texture path.
        adt->textures = {"Tileset\\Elwynn\\ElwynnGrassBase.blp"};

        TerrainCellInput terrainInput;
        terrainInput.heights.fill(50.0f);
        terrainInput.normals.fill(TerrainNormal{0.0f, 1.0f, 0.0f});
        TerrainLayerInput baseLayer;
        baseLayer.textureId = 0;
        terrainInput.layers.push_back(baseLayer);
        const SerializedTerrain terrain = SerializeLegacyTerrain(terrainInput, adt->textures.size());

        constexpr float chunkSize = 100.0f / 3.0f;
        for (std::uint32_t y = 0; y < 16; ++y)
        {
            for (std::uint32_t x = 0; x < 16; ++x)
            {
                AdtCellInput& cell = adt->cells[y * 16u + x];
                cell.header.ix = x;
                cell.header.iy = y;

                // Noggit reconstructs the tile-32,32 chunk positions as
                // ZEROPOINT - (32*TILESIZE + index*CHUNKSIZE), reducing here
                // to -index*CHUNKSIZE for both horizontal axes.
                cell.header.x = -static_cast<float>(x) * chunkSize;
                cell.header.z = -static_cast<float>(y) * chunkSize;
                cell.header.areaId = 0;
                ApplyTerrainToMcnk(terrain, cell.header, cell.subchunks);
            }
        }

        const SerializedAdt serialized = SerializeVanillaAdt(*adt);
        ValidateVanillaAdtRoot(serialized.bytes);

        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot create output ADT: " + outputPath);
        out.write(reinterpret_cast<const char*>(serialized.bytes.data()),
                  static_cast<std::streamsize>(serialized.bytes.size()));
        if (!out)
            throw std::runtime_error("failed while writing output ADT: " + outputPath);

        std::cout << "Fixture A written: " << outputPath
                  << " (" << serialized.bytes.size() << " bytes)\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fixture A generation failed: " << e.what() << '\n';
        return 1;
    }
}
