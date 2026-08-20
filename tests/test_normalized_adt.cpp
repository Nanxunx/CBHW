#include "turtle335/adt/AdtValidator.h"
#include "turtle335/adt/NormalizedAdt.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>

using namespace turtle335::adt;

static std::uint32_t ReadLe32(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

static LiquidLayer MakeWater(float height)
{
    LiquidLayer layer;
    layer.category = LiquidCategory::Water;
    layer.offsetX = 0;
    layer.offsetY = 0;
    layer.width = 1;
    layer.height = 1;
    layer.visible = {true};
    layer.vertices.resize(4);
    for (LiquidVertex& vertex : layer.vertices)
    {
        vertex.height = height;
        vertex.depth = 96;
    }
    return layer;
}

static std::unique_ptr<NormalizedAdt> MakeBaseAdt()
{
    auto adt = std::make_unique<NormalizedAdt>();
    adt->textures = {"Tileset\\Elwynn\\ElwynnGrassBase.blp"};

    M2PlacementInput m2;
    m2.assetPath = "World\\Generic\\Test\\SemanticTree.m2";
    m2.uniqueId = 1001;
    m2.position = {1.0f, 2.0f, 3.0f};
    adt->m2Placements.push_back(m2);

    WmoPlacementInput wmo;
    wmo.assetPath = "World\\Wmo\\Test\\SemanticHouse.wmo";
    wmo.uniqueId = 2001;
    wmo.minimumExtent = {-5.0f, -5.0f, -5.0f};
    wmo.maximumExtent = {5.0f, 5.0f, 5.0f};
    adt->wmoPlacements.push_back(wmo);

    constexpr float chunkSize = 100.0f / 3.0f;
    for (std::uint32_t y = 0; y < 16; ++y)
    {
        for (std::uint32_t x = 0; x < 16; ++x)
        {
            NormalizedAdtCell& cell = adt->cells[y * 16u + x];
            cell.ix = x;
            cell.iy = y;
            cell.areaId = 12;
            cell.positionX = -static_cast<float>(x) * chunkSize;
            cell.positionZ = -static_cast<float>(y) * chunkSize;
            cell.terrain.heights.fill(50.0f);
            cell.terrain.normals.fill(TerrainNormal{0.0f, 1.0f, 0.0f});
            TerrainLayerInput base;
            base.textureId = 0;
            cell.terrain.layers.push_back(base);
        }
    }

    adt->cells[0].m2Refs = {0};
    adt->cells[0].wmoRefs = {0};
    adt->cells[0].liquids.push_back(MakeWater(51.0f));
    return adt;
}

int main()
{
    {
        auto input = MakeBaseAdt();
        const NormalizedAdtBuildResult built = SerializeNormalizedAdt(*input);
        assert(built.lossless);
        assert(built.diagnostics.empty());

        const AdtValidationReport report = ValidateVanillaAdt(built.adt.bytes);
        assert(report.version == 18);
        assert(report.textureCount == 1);
        assert(report.m2PlacementCount == 1);
        assert(report.wmoPlacementCount == 1);
        assert(report.mcnkCount == 256);
        assert(report.liquidRecordCount == 1);

        const std::size_t firstMcnk = built.adt.layout.mcnkOffsets[0];
        const std::uint32_t flags = ReadLe32(built.adt.bytes.data() + firstMcnk + 8u);
        const std::uint32_t nDoodadRefs = ReadLe32(built.adt.bytes.data() + firstMcnk + 24u);
        const std::uint32_t nMapObjRefs = ReadLe32(built.adt.bytes.data() + firstMcnk + 64u);
        const std::uint32_t sizeMclq = ReadLe32(built.adt.bytes.data() + firstMcnk + 108u);
        assert((flags & 0x04u) != 0);
        assert((flags & (1u << 15)) != 0); // production NormalizedAdt emits full 64x64 MCAL/MCSH edges
        assert(nDoodadRefs == 1);
        assert(nMapObjRefs == 1);
        assert(sizeMclq == 812u);
    }

    // Same-category overlapping MH2O layers remain serializable but must be
    // surfaced as a lossy legacy representation instead of being silent.
    {
        auto input = MakeBaseAdt();
        input->cells[0].liquids.push_back(MakeWater(60.0f));
        const NormalizedAdtBuildResult built = SerializeNormalizedAdt(*input);
        assert(!built.lossless);
        assert(!built.diagnostics.empty());
        assert(built.diagnostics.front().cellSlot == 0);
        (void)ValidateVanillaAdt(built.adt.bytes);
    }

    std::cout << "turtle335_normalized_adt_tests: OK\n";
    return 0;
}
