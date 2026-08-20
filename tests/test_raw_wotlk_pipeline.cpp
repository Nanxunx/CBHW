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

static std::uint32_t ReadLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

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
    constexpr std::size_t table = 256u * 12u;
    constexpr std::size_t instance = table;
    constexpr std::size_t attributes = instance + 24u;
    constexpr std::size_t exists = attributes + 16u;
    constexpr std::size_t vertices = exists + 8u;
    constexpr std::size_t vertexCount = 4u;
    constexpr std::size_t payloadSize = vertices + vertexCount * 4u + vertexCount;

    std::vector<std::uint8_t> chunk(8u + payloadSize, 0);
    std::memcpy(chunk.data(), "O2HM", 4);
    WriteLe32(chunk, 4, static_cast<std::uint32_t>(payloadSize));
    const std::size_t b = 8;

    WriteLe32(chunk, b + 0, static_cast<std::uint32_t>(instance));
    WriteLe32(chunk, b + 4, 1);
    WriteLe32(chunk, b + 8, static_cast<std::uint32_t>(attributes));
    WriteLe16(chunk, b + instance + 0, 17);
    WriteLe16(chunk, b + instance + 2, 0);
    WriteLeF32(chunk, b + instance + 4, 61.0f);
    WriteLeF32(chunk, b + instance + 8, 62.0f);
    chunk[b + instance + 14] = 1;
    chunk[b + instance + 15] = 1;
    WriteLe32(chunk, b + instance + 16, static_cast<std::uint32_t>(exists));
    WriteLe32(chunk, b + instance + 20, static_cast<std::uint32_t>(vertices));
    WriteLe64(chunk, b + attributes + 0, 1);
    WriteLe64(chunk, b + attributes + 8, 0);
    WriteLe64(chunk, b + exists, 1);
    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        WriteLeF32(chunk, b + vertices + i * 4u, 61.0f + static_cast<float>(i) * 0.1f);
        chunk[b + vertices + vertexCount * 4u + i] = 120;
    }
    return chunk;
}

static std::unique_ptr<NormalizedAdt> MakeDryAdt()
{
    auto adt = std::make_unique<NormalizedAdt>();
    adt->textures = {"Tileset\\Test\\RawPipeline.blp"};
    for (std::uint32_t y = 0; y < 16; ++y)
    {
        for (std::uint32_t x = 0; x < 16; ++x)
        {
            auto& cell = adt->cells[y * 16u + x];
            cell.ix = x;
            cell.iy = y;
            cell.areaId = 33;
            cell.positionX = -static_cast<float>(x) * (100.0f / 3.0f);
            cell.positionZ = -static_cast<float>(y) * (100.0f / 3.0f);
            cell.terrain.heights.fill(60.0f);
            cell.terrain.normals.fill(TerrainNormal{0.0f, 1.0f, 0.0f});
            TerrainLayerInput base;
            base.textureId = 0;
            cell.terrain.layers.push_back(base);
        }
    }
    return adt;
}

static std::vector<std::uint8_t> InjectMh2o(const SerializedAdt& source,
                                            const std::vector<std::uint8_t>& mh2o)
{
    const std::size_t insertion = source.layout.mcnkOffsets[0];
    std::vector<std::uint8_t> bytes;
    bytes.reserve(source.bytes.size() + mh2o.size());
    bytes.insert(bytes.end(), source.bytes.begin(), source.bytes.begin() + static_cast<std::ptrdiff_t>(insertion));
    bytes.insert(bytes.end(), mh2o.begin(), mh2o.end());
    bytes.insert(bytes.end(), source.bytes.begin() + static_cast<std::ptrdiff_t>(insertion), source.bytes.end());

    // MVER is 12 bytes; MHDR is the next 72. MHDR payload starts at byte 20,
    // and its MH2O pointer field is payload+0x28. Offsets are relative to MHDR
    // payload start, not file start.
    constexpr std::size_t mhdrPayload = 20u;
    WriteLe32(bytes, mhdrPayload + 0x28u,
              static_cast<std::uint32_t>(insertion - mhdrPayload));

    // MCIN itself is before insertion. Every MCNK absolute offset moves by the
    // inserted MH2O byte count; sizes remain unchanged.
    const std::size_t mcinPayload = source.layout.mcinOffset + 8u;
    for (std::size_t entry = 0; entry < 256; ++entry)
    {
        const std::size_t at = mcinPayload + entry * 16u;
        const std::uint32_t oldOffset = ReadLe32(bytes, at);
        WriteLe32(bytes, at, oldOffset + static_cast<std::uint32_t>(mh2o.size()));
    }
    return bytes;
}

int main()
{
    const auto semantic = MakeDryAdt();
    const NormalizedAdtBuildResult dry = SerializeNormalizedAdt(*semantic);
    const std::vector<std::uint8_t> sourceBytes = InjectMh2o(dry.adt, BuildMh2o());

    const WotlkAdtDocument parsed = ParseWotlkAdt(sourceBytes);
    assert(!parsed.mh2o.empty());
    assert(parsed.cells[0].header.ix == 0);
    assert(parsed.cells[255].header.ix == 15);
    assert(parsed.cells[255].header.iy == 15);

    const WotlkAdtNormalizationResult normalized = NormalizeWotlkAdt(
        parsed,
        [](std::uint16_t id) {
            assert(id == 17);
            return LiquidCategory::Water;
        },
        false);
    assert(normalized.ready);
    assert(normalized.lossless);
    assert(normalized.adt.cells[0].liquids.size() == 1);

    const NormalizedAdtBuildResult target = SerializeNormalizedAdt(normalized.adt);
    assert(target.lossless);
    const AdtValidationReport report = ValidateVanillaAdt(target.adt.bytes);
    assert(report.mcnkCount == 256);
    assert(report.textureCount == 1);
    assert(report.liquidRecordCount == 1);

    const WotlkAdtDocument targetParsed = ParseWotlkAdt(target.adt.bytes);
    assert(targetParsed.mh2o.empty());
    assert((targetParsed.cells[0].header.flags & 0x04u) != 0);
    assert(targetParsed.cells[0].mclq.size() == 812u);

    std::cout << "turtle335_raw_wotlk_pipeline_tests: OK\n";
    return 0;
}
