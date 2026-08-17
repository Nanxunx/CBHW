#include "turtle335/adt/Holes.h"
#include "turtle335/adt/LegacyLiquid.h"
#include "turtle335/adt/Mcal.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
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

int main()
{
    TestHoles();
    TestMcalQuantization();
    TestMcalRle();
    TestLiquidNonOverlappingMerge();
    TestLiquidOverlapDiagnostic();
    TestLiquidSharedVertexConflict();
    TestLiquidSharedVertexPayloadConflict();
    std::cout << "turtle335_core_tests: OK\n";
    return 0;
}
