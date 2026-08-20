#include "turtle335/adt/AdtValidator.h"
#include "turtle335/adt/NormalizedAdt.h"
#include "turtle335/adt/WotlkAdtNormalizer.h"
#include "turtle335/adt/WotlkAdtReader.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace turtle335::adt;

static void WriteLe16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

static void WriteLe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

static void WriteLe64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i)
        bytes[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu);
}

static void WriteLeF32(std::vector<std::uint8_t>& bytes, std::size_t offset, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteLe32(bytes, offset, bits);
}

static std::vector<std::uint8_t> BuildMh2o()
{
    constexpr std::size_t headerTable = 256u * 12u;
    constexpr std::size_t instanceOff = headerTable;
    constexpr std::size_t attrOff = instanceOff + 24u;
    constexpr std::size_t existsOff = attrOff + 16u;
    constexpr std::size_t vertexOff = existsOff + 8u;
    constexpr std::size_t vertexCount = 4u;
    constexpr std::size_t payloadSize = vertexOff + vertexCount * 4u + vertexCount;

    std::vector<std::uint8_t> chunk(8u + payloadSize, 0);
    std::memcpy(chunk.data(), "O2HM", 4);
    WriteLe32(chunk, 4, static_cast<std::uint32_t>(payloadSize));
    const std::size_t base = 8;

    WriteLe32(chunk, base + 0, static_cast<std::uint32_t>(instanceOff));
    WriteLe32(chunk, base + 4, 1);
    WriteLe32(chunk, base + 8, static_cast<std::uint32_t>(attrOff));

    WriteLe16(chunk, base + instanceOff + 0, 7);
    WriteLe16(chunk, base + instanceOff + 2, 0);
    WriteLeF32(chunk, base + instanceOff + 4, 51.0f);
    WriteLeF32(chunk, base + instanceOff + 8, 52.0f);
    chunk[base + instanceOff + 12] = 0;
    chunk[base + instanceOff + 13] = 0;
    chunk[base + instanceOff + 14] = 1;
    chunk[base + instanceOff + 15] = 1;
    WriteLe32(chunk, base + instanceOff + 16, static_cast<std::uint32_t>(existsOff));
    WriteLe32(chunk, base + instanceOff + 20, static_cast<std::uint32_t>(vertexOff));

    WriteLe64(chunk, base + attrOff + 0, 1);
    WriteLe64(chunk, base + attrOff + 8, 1);
    WriteLe64(chunk, base + existsOff, 1);

    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        WriteLeF32(chunk, base + vertexOff + i * 4u, 51.0f + static_cast<float>(i) * 0.25f);
        chunk[base + vertexOff + vertexCount * 4u + i] = static_cast<std::uint8_t>(100u + i);
    }
    return chunk;
}

static std::vector<std::uint8_t> BuildMcsh()
{
    std::vector<std::uint8_t> chunk(8u + 512u, 0);
    std::memcpy(chunk.data(), "HSCM", 4);
    WriteLe32(chunk, 4, 512u);
    // Row 10 / column 62. With source bit15 clear the normalizer must copy it
    // into column 63 before handing the full-edge chunk to the target writer.
    const std::size_t bit = 10u * 64u + 62u;
    chunk[8u + bit / 8u] |= static_cast<std::uint8_t>(1u << (bit % 8u));
    return chunk;
}

static bool McshBit(const std::vector<std::uint8_t>& chunk, std::size_t x, std::size_t y)
{
    const std::size_t bit = y * 64u + x;
    return ((chunk[8u + bit / 8u] >> (bit % 8u)) & 1u) != 0;
}

static std::unique_ptr<NormalizedAdt> MakeDrySemanticSource()
{
    auto adt = std::make_unique<NormalizedAdt>();
    adt->textures = {"Tileset\\Test\\Base.blp"};

    M2PlacementInput placement;
    placement.assetPath = "World\\Generic\\Test\\Tree.m2";
    placement.uniqueId = 500;
    adt->m2Placements.push_back(placement);

    constexpr float chunkSize = 100.0f / 3.0f;
    for (std::uint32_t y = 0; y < 16; ++y)
    {
        for (std::uint32_t x = 0; x < 16; ++x)
        {
            NormalizedAdtCell& cell = adt->cells[y * 16u + x];
            cell.ix = x;
            cell.iy = y;
            cell.areaId = 7;
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
    return adt;
}

static WotlkAdtDocument MakeSourceDocument()
{
    const std::unique_ptr<NormalizedAdt> semantic = MakeDrySemanticSource();
    const NormalizedAdtBuildResult built = SerializeNormalizedAdt(*semantic);
    WotlkAdtDocument source = ParseWotlkAdt(built.adt.bytes);
    source.mh2o = BuildMh2o();
    return source;
}

int main()
{
    {
        const WotlkAdtDocument source = MakeSourceDocument();
        const WotlkAdtNormalizationResult normalized = NormalizeWotlkAdt(
            source,
            [](std::uint16_t id) {
                assert(id == 7);
                return LiquidCategory::Ocean;
            },
            false);

        assert(normalized.ready);
        assert(normalized.lossless);
        assert(normalized.issues.empty());
        assert(normalized.adt.m2Placements.size() == 1);
        assert(normalized.adt.cells[0].m2Refs.size() == 1);
        assert(normalized.adt.cells[0].liquids.size() == 1);
        assert(normalized.adt.cells[0].liquids[0].category == LiquidCategory::Ocean);
        assert(normalized.adt.cells[0].legacy3E == 0);
        assert(normalized.adt.cells[0].predTex == 0);
        assert(normalized.adt.cells[0].nEffectDoodad == 0);

        const NormalizedAdtBuildResult target = SerializeNormalizedAdt(normalized.adt);
        assert(target.lossless);
        const AdtValidationReport report = ValidateVanillaAdt(target.adt.bytes);
        assert(report.mcnkCount == 256);
        assert(report.m2PlacementCount == 1);
        assert(report.liquidRecordCount == 1);
    }

    {
        WotlkAdtDocument source = MakeSourceDocument();
        source.cells[0].header.flags &= ~(1u << 15);
        source.cells[0].header.flags |= 0x01u;
        source.cells[0].mcsh = BuildMcsh();
        const auto normalized = NormalizeWotlkAdt(
            source,
            [](std::uint16_t) { return LiquidCategory::Ocean; },
            false);
        assert(normalized.ready);
        assert(normalized.lossless);
        assert(normalized.adt.cells[0].targetMcsh.size() == 520u);
        assert(McshBit(normalized.adt.cells[0].targetMcsh, 62, 10));
        assert(McshBit(normalized.adt.cells[0].targetMcsh, 63, 10));

        const NormalizedAdtBuildResult target = SerializeNormalizedAdt(normalized.adt);
        const WotlkAdtDocument reparsed = ParseWotlkAdt(target.adt.bytes);
        assert(reparsed.cells[0].mcsh.size() == 520u);
        assert((reparsed.cells[0].header.flags & (1u << 15)) != 0);
        assert(McshBit(reparsed.cells[0].mcsh, 63, 10));
    }

    {
        WotlkAdtDocument source = MakeSourceDocument();
        const auto normalized = NormalizeWotlkAdt(
            source,
            [](std::uint16_t) { return LiquidCategory::Unknown; },
            false);
        assert(!normalized.ready);
        assert(!normalized.lossless);
    }

    {
        WotlkAdtDocument source = MakeSourceDocument();
        source.cells[0].header.flags |= (1u << 16);
        const auto normalized = NormalizeWotlkAdt(
            source,
            [](std::uint16_t) { return LiquidCategory::Ocean; },
            false);
        assert(!normalized.ready);
    }

    {
        WotlkAdtDocument source = MakeSourceDocument();
        source.cells[0].header.legacy3E = 9;
        const auto normalized = NormalizeWotlkAdt(
            source,
            [](std::uint16_t) { return LiquidCategory::Ocean; },
            false);
        assert(normalized.ready);
        assert(!normalized.lossless);
        assert(normalized.adt.cells[0].legacy3E == 0);
    }

    std::cout << "turtle335_wotlk_adt_normalizer_tests: OK\n";
    return 0;
}
