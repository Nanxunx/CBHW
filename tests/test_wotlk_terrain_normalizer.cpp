#include "turtle335/adt/Mcal.h"
#include "turtle335/adt/WotlkTerrainNormalizer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace turtle335::adt;

static void WriteLe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

static void WriteLeF32(std::vector<std::uint8_t>& bytes, std::size_t offset, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteLe32(bytes, offset, bits);
}

static std::vector<std::uint8_t> MakeChunk(const char id[4], const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> out(8u + payload.size(), 0);
    std::memcpy(out.data(), id, 4);
    WriteLe32(out, 4, static_cast<std::uint32_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), out.begin() + 8);
    return out;
}

static WotlkMcnkRecord MakeTerrainCell(std::size_t layerCount)
{
    WotlkMcnkRecord cell;
    cell.header.nLayers = static_cast<std::uint32_t>(layerCount);
    cell.header.y = 100.0f;

    std::vector<std::uint8_t> mcvt(145u * 4u, 0);
    WriteLeF32(mcvt, 4u, 2.5f);
    cell.mcvt = MakeChunk("TVCM", mcvt);

    std::vector<std::uint8_t> mcnr(145u * 3u, 0);
    for (std::size_t i = 0; i < 145; ++i)
        mcnr[i * 3u + 2u] = 127u; // disk order x,z,y -> semantic normal 0,1,0
    cell.mcnr = MakeChunk("RNCM", mcnr);

    return cell;
}

static void SetLayer(std::vector<std::uint8_t>& payload,
                     std::size_t index,
                     std::uint32_t textureId,
                     std::uint32_t flags,
                     std::uint32_t alphaOffset,
                     std::uint32_t effectId)
{
    const std::size_t at = index * 16u;
    WriteLe32(payload, at + 0u, textureId);
    WriteLe32(payload, at + 4u, flags);
    WriteLe32(payload, at + 8u, alphaOffset);
    WriteLe32(payload, at + 12u, effectId);
}

static void TestLegacySequentialAlpha()
{
    WotlkMcnkRecord cell = MakeTerrainCell(3);
    std::vector<std::uint8_t> mcly(3u * 16u, 0);
    SetLayer(mcly, 0, 0, 0, 0, 10);
    SetLayer(mcly, 1, 1, 0x100u | 0x40u, 0, 11);
    SetLayer(mcly, 2, 2, 0x100u | 0x400u, 2048, 12);
    cell.mcly = MakeChunk("YLCM", mcly);

    Alpha8 sequentialA{};
    Alpha8 sequentialB{};
    sequentialA.fill(136);
    sequentialB.fill(68);
    const Alpha4Packed packedA = EncodeLegacyAlpha4(sequentialA);
    const Alpha4Packed packedB = EncodeLegacyAlpha4(sequentialB);
    std::vector<std::uint8_t> mcal;
    mcal.insert(mcal.end(), packedA.begin(), packedA.end());
    mcal.insert(mcal.end(), packedB.begin(), packedB.end());
    cell.mcal = MakeChunk("LACM", mcal);

    const WotlkTerrainNormalizationResult result = NormalizeWotlkTerrain(cell, 3, false);
    assert(result.alphaStorage == WotlkAlphaStorage::Legacy4Sequential);
    assert(result.terrain.layers.size() == 3);
    assert(result.terrain.heights[0] == 100.0f);
    assert(std::abs(result.terrain.heights[1] - 102.5f) < 1.0e-6f);
    assert(std::abs(result.terrain.normals[0].x) < 1.0e-6f);
    assert(std::abs(result.terrain.normals[0].y - 1.0f) < 1.0e-6f);
    assert(std::abs(result.terrain.normals[0].z) < 1.0e-6f);
    assert(result.terrain.layers[1].flags == 0x40u);
    assert(result.terrain.layers[2].flags == 0x400u);
    assert((*result.terrain.layers[1].alpha)[0] == 100u);
    assert((*result.terrain.layers[2].alpha)[0] == 68u);
}

static void TestBigAlpha()
{
    WotlkMcnkRecord cell = MakeTerrainCell(2);
    std::vector<std::uint8_t> mcly(2u * 16u, 0);
    SetLayer(mcly, 0, 0, 0, 0, 0xFFFFu);
    SetLayer(mcly, 1, 1, 0x100u, 0, 0xFFFFu);
    cell.mcly = MakeChunk("YLCM", mcly);

    std::vector<std::uint8_t> alpha(4096u, 200u);
    cell.mcal = MakeChunk("LACM", alpha);

    const auto result = NormalizeWotlkTerrain(cell, 2, true);
    assert(result.alphaStorage == WotlkAlphaStorage::Big8Independent);
    assert((*result.terrain.layers[1].alpha)[1234] == 200u);
}

static void TestRleAlpha()
{
    WotlkMcnkRecord cell = MakeTerrainCell(2);
    std::vector<std::uint8_t> mcly(2u * 16u, 0);
    SetLayer(mcly, 0, 0, 0, 0, 0xFFFFu);
    SetLayer(mcly, 1, 1, 0x100u | 0x200u, 0, 0xFFFFu);
    cell.mcly = MakeChunk("YLCM", mcly);

    std::vector<std::uint8_t> rle;
    for (int row = 0; row < 64; ++row)
    {
        rle.push_back(static_cast<std::uint8_t>(0x80u | 64u));
        rle.push_back(77u);
    }
    cell.mcal = MakeChunk("LACM", rle);

    const auto result = NormalizeWotlkTerrain(cell, 2, true);
    assert(result.alphaStorage == WotlkAlphaStorage::Rle8Independent);
    assert(std::all_of(result.terrain.layers[1].alpha->begin(),
                       result.terrain.layers[1].alpha->end(),
                       [](std::uint8_t value) { return value == 77u; }));
}

static void TestMixedStorageRejected()
{
    WotlkMcnkRecord cell = MakeTerrainCell(3);
    std::vector<std::uint8_t> mcly(3u * 16u, 0);
    SetLayer(mcly, 0, 0, 0, 0, 0xFFFFu);
    SetLayer(mcly, 1, 1, 0x100u, 0, 0xFFFFu);
    SetLayer(mcly, 2, 2, 0x100u, 2048u, 0xFFFFu);
    cell.mcly = MakeChunk("YLCM", mcly);

    Alpha8 old{};
    old.fill(85u);
    const Alpha4Packed packed = EncodeLegacyAlpha4(old);
    std::vector<std::uint8_t> payload(packed.begin(), packed.end());
    payload.insert(payload.end(), 4096u, 42u);
    cell.mcal = MakeChunk("LACM", payload);

    bool rejected = false;
    try { (void)NormalizeWotlkTerrain(cell, 3, true); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
}

static void TestInvisibleNonBaseLayer()
{
    WotlkMcnkRecord cell = MakeTerrainCell(2);
    std::vector<std::uint8_t> mcly(2u * 16u, 0);
    SetLayer(mcly, 0, 0, 0, 0, 0xFFFFu);
    SetLayer(mcly, 1, 1, 0, 0, 0xFFFFu);
    cell.mcly = MakeChunk("YLCM", mcly);

    const auto result = NormalizeWotlkTerrain(cell, 2, false);
    assert(result.alphaStorage == WotlkAlphaStorage::None);
    assert(result.terrain.layers[1].alpha.has_value());
    assert(std::all_of(result.terrain.layers[1].alpha->begin(),
                       result.terrain.layers[1].alpha->end(),
                       [](std::uint8_t value) { return value == 0u; }));
}

int main()
{
    TestLegacySequentialAlpha();
    TestBigAlpha();
    TestRleAlpha();
    TestMixedStorageRejected();
    TestInvisibleNonBaseLayer();
    std::cout << "turtle335_wotlk_terrain_normalizer_tests: OK\n";
    return 0;
}
