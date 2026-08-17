#include "turtle335/adt/Holes.h"
#include "turtle335/adt/LegacyLiquid.h"
#include "turtle335/adt/Mcal.h"
#include "turtle335/adt/MclqWriter.h"
#include "turtle335/adt/Mh2oReader.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace turtle335::adt;

static LiquidLayer MakeFlatLayer(LiquidCategory cat, int x, int y, int w, int h, float z)
{
    LiquidLayer l;
    l.category = cat;
    l.offsetX = static_cast<std::uint8_t>(x);
    l.offsetY = static_cast<std::uint8_t>(y);
    l.width = static_cast<std::uint8_t>(w);
    l.height = static_cast<std::uint8_t>(h);
    l.visible.assign(static_cast<std::size_t>(w * h), true);
    l.vertices.resize(static_cast<std::size_t>((w + 1) * (h + 1)));
    for (auto& v : l.vertices)
    {
        v.height = z;
        v.depth = 127;
    }
    return l;
}

static void TestHoles()
{
    for (int bit = 0; bit < 16; ++bit)
    {
        const std::uint16_t mask = static_cast<std::uint16_t>(1u << bit);
        const int bx = bit % 4;
        const int by = bit / 4;
        for (int y = 0; y < 8; ++y)
        {
            for (int x = 0; x < 8; ++x)
            {
                const bool expected = (x / 2 == bx) && (y / 2 == by);
                assert(IsQuadHole(mask, x, y) == expected);
            }
        }
    }
}

static void TestMcalQuantization()
{
    for (int a = 0; a <= 255; ++a)
    {
        const auto q = Quantize8To4(static_cast<std::uint8_t>(a));
        const auto rebuilt = Expand4To8(q);
        assert(std::abs(a - int(rebuilt)) <= 8);
    }

    Alpha8 alpha{};
    for (std::size_t i = 0; i < alpha.size(); ++i)
        alpha[i] = static_cast<std::uint8_t>(i % 256);

    const auto packed = EncodeLegacyAlpha4(alpha);
    const auto decoded = DecodeLegacyAlpha4(packed.data(), packed.size());
    for (std::size_t i = 0; i < alpha.size(); ++i)
        assert(std::abs(int(alpha[i]) - int(decoded[i])) <= 8);
}

static void TestMcalRle()
{
    std::vector<std::uint8_t> encoded;
    for (int i = 0; i < 32; ++i)
    {
        encoded.push_back(0x80 | 127);
        encoded.push_back(42);
    }
    encoded.push_back(0x80 | 32);
    encoded.push_back(42);

    const auto decoded = DecodeRleAlpha8(encoded.data(), encoded.size());
    assert(std::all_of(decoded.begin(), decoded.end(), [](std::uint8_t v) { return v == 42; }));
}

static void TestLiquidNonOverlappingMerge()
{
    auto water = MakeFlatLayer(LiquidCategory::Water, 0, 0, 3, 3, 10.0f);
    auto ocean = MakeFlatLayer(LiquidCategory::Ocean, 5, 5, 3, 3, 20.0f);
    ocean.deepMask = (std::uint64_t{1} << (5 * 8 + 5));

    const auto r = BuildLegacyMclq({water, ocean});
    assert(r.mclq.has_value());
    assert(r.lossless);
    assert(r.mclq->cellFlags[0] == 0x04);
    assert(r.mclq->cellFlags[5 * 8 + 5] == (0x01 | 0x80));
    assert((r.mclq->mcnkLiquidFlags & 0x0C) == 0x0C);
    assert(r.mclq->minHeight == 10.0f);
    assert(r.mclq->maxHeight == 20.0f);
}

static void TestLiquidOverlapDiagnostic()
{
    auto a = MakeFlatLayer(LiquidCategory::Water, 0, 0, 2, 2, 10.0f);
    auto b = MakeFlatLayer(LiquidCategory::Ocean, 1, 1, 2, 2, 10.0f);
    const auto r = BuildLegacyMclq({a, b});
    assert(r.mclq.has_value());
    assert(!r.lossless);
    assert(std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [](const LiquidDiagnostic& d) {
        return d.kind == LiquidDiagnosticKind::OverlappingCells;
    }));
}

static void TestLiquidSharedVertexConflict()
{
    auto a = MakeFlatLayer(LiquidCategory::Water, 0, 0, 1, 1, 10.0f);
    auto b = MakeFlatLayer(LiquidCategory::Water, 1, 0, 1, 1, 20.0f);
    const auto r = BuildLegacyMclq({a, b});
    assert(r.mclq.has_value());
    assert(!r.lossless);
    assert(std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [](const LiquidDiagnostic& d) {
        return d.kind == LiquidDiagnosticKind::SharedVertexHeightConflict;
    }));
}

static void TestLiquidSharedVertexPayloadConflict()
{
    auto water = MakeFlatLayer(LiquidCategory::Water, 0, 0, 1, 1, 10.0f);
    auto magma = MakeFlatLayer(LiquidCategory::Magma, 1, 0, 1, 1, 10.0f);
    for (auto& v : magma.vertices)
    {
        v.depth.reset();
        v.u = 100;
        v.v = 200;
    }
    const auto r = BuildLegacyMclq({water, magma});
    assert(r.mclq.has_value());
    assert(!r.lossless);
    assert(std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [](const LiquidDiagnostic& d) {
        return d.kind == LiquidDiagnosticKind::SharedVertexPayloadConflict;
    }));
}

static std::uint32_t ReadLe32(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

static void WriteLe16(std::vector<std::uint8_t>& b, std::size_t o, std::uint16_t v)
{
    b[o] = std::uint8_t(v & 0xFFu);
    b[o + 1] = std::uint8_t(v >> 8);
}

static void WriteLe32(std::vector<std::uint8_t>& b, std::size_t o, std::uint32_t v)
{
    b[o] = std::uint8_t(v & 0xFFu);
    b[o + 1] = std::uint8_t((v >> 8) & 0xFFu);
    b[o + 2] = std::uint8_t((v >> 16) & 0xFFu);
    b[o + 3] = std::uint8_t((v >> 24) & 0xFFu);
}

static void WriteLe64(std::vector<std::uint8_t>& b, std::size_t o, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        b[o + i] = std::uint8_t((v >> (i * 8)) & 0xFFu);
}

static void WriteLeF32(std::vector<std::uint8_t>& b, std::size_t o, float f)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(f));
    std::memcpy(&bits, &f, sizeof(bits));
    WriteLe32(b, o, bits);
}

static void TestMclqSerialization()
{
    auto water = MakeFlatLayer(LiquidCategory::Water, 0, 0, 1, 1, 12.5f);
    const auto built = BuildLegacyMclq({water});
    assert(built.mclq.has_value());

    const auto payload = SerializeLegacyMclqPayload(*built.mclq);
    const auto chunk = SerializeLegacyMclqChunk(*built.mclq);
    static_assert(payload.size() == 804);
    static_assert(chunk.size() == 812);
    assert(chunk[0] == 'Q' && chunk[1] == 'L' && chunk[2] == 'C' && chunk[3] == 'M');
    assert(ReadLe32(chunk.data() + 4) == 804);
    assert(std::equal(payload.begin(), payload.end(), chunk.begin() + 8));

    const std::size_t flagsOffset = 8 + 81 * 8;
    assert(payload[flagsOffset] == 0x04);
    assert(payload[flagsOffset + 1] == 0x0F);

    const std::size_t flowOffset = flagsOffset + 64;
    assert(ReadLe32(payload.data() + flowOffset) == 0);
}

static void TestMh2oReaderHeightDepth()
{
    constexpr std::size_t headerTable = 256 * 12;
    constexpr std::size_t instanceOff = headerTable;
    constexpr std::size_t attrOff = instanceOff + 24;
    constexpr std::size_t existsOff = attrOff + 16;
    constexpr std::size_t vertexOff = existsOff + 8;
    constexpr std::size_t vertexCount = 4;
    constexpr std::size_t payloadSize = vertexOff + vertexCount * 4 + vertexCount;

    std::vector<std::uint8_t> chunk(8 + payloadSize, 0);
    chunk[0] = 'O'; chunk[1] = '2'; chunk[2] = 'H'; chunk[3] = 'M';
    WriteLe32(chunk, 4, payloadSize);

    const std::size_t base = 8;
    WriteLe32(chunk, base + 0, instanceOff);
    WriteLe32(chunk, base + 4, 1);
    WriteLe32(chunk, base + 8, attrOff);

    WriteLe16(chunk, base + instanceOff + 0, 7);
    WriteLe16(chunk, base + instanceOff + 2, 0);
    WriteLeF32(chunk, base + instanceOff + 4, 10.0f);
    WriteLeF32(chunk, base + instanceOff + 8, 13.0f);
    chunk[base + instanceOff + 12] = 2;
    chunk[base + instanceOff + 13] = 3;
    chunk[base + instanceOff + 14] = 1;
    chunk[base + instanceOff + 15] = 1;
    WriteLe32(chunk, base + instanceOff + 16, existsOff);
    WriteLe32(chunk, base + instanceOff + 20, vertexOff);

    WriteLe64(chunk, base + attrOff, std::uint64_t{1} << (3 * 8 + 2));
    WriteLe64(chunk, base + attrOff + 8, std::uint64_t{1} << (3 * 8 + 2));
    WriteLe64(chunk, base + existsOff, 1);

    const float heights[4] = {10.0f, 11.0f, 12.0f, 13.0f};
    for (std::size_t i = 0; i < 4; ++i)
        WriteLeF32(chunk, base + vertexOff + i * 4, heights[i]);
    chunk[base + vertexOff + 16] = 10;
    chunk[base + vertexOff + 17] = 20;
    chunk[base + vertexOff + 18] = 30;
    chunk[base + vertexOff + 19] = 40;

    const auto parsed = ParseMh2oChunk(chunk.data(), chunk.size(), [](std::uint16_t id) {
        assert(id == 7);
        return LiquidCategory::Ocean;
    });

    assert(parsed.chunks[0].size() == 1);
    const auto& p = parsed.chunks[0][0];
    assert(p.metadata.sourceLiquidType == 7);
    assert(p.metadata.sourceVertexFormat == Mh2oVertexFormat::HeightDepth);
    assert(p.layer.category == LiquidCategory::Ocean);
    assert(p.layer.offsetX == 2 && p.layer.offsetY == 3);
    assert(p.layer.visible.size() == 1 && p.layer.visible[0]);
    assert(p.layer.vertices.size() == 4);
    assert(p.layer.vertices[3].height == 13.0f);
    assert(p.layer.vertices[3].depth == 40);
    assert((p.layer.deepMask >> (3 * 8 + 2)) & 1u);
    assert((p.layer.fishableMask >> (3 * 8 + 2)) & 1u);
}

int main()
{
    TestHoles();
    TestMcalQuantization();
    TestMcalRle();
    TestLiquidNonOverlappingMerge();
    TestLiquidOverlapDiagnostic();
    TestLiquidSharedVertexConflict();
    TestLiquidSharedVertexPayloadConflict();
    TestMclqSerialization();
    TestMh2oReaderHeightDepth();
    std::cout << "turtle335_core_tests: OK\n";
    return 0;
}
