#include "turtle335/adt/LegacyLiquid.h"
#include "turtle335/adt/McnkWriter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace turtle335::adt;

static std::uint32_t ReadLe32(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) |
           (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

static void WriteLe32(std::vector<std::uint8_t>& b, std::size_t o, std::uint32_t v)
{
    b[o] = std::uint8_t(v & 0xFFu);
    b[o + 1] = std::uint8_t((v >> 8) & 0xFFu);
    b[o + 2] = std::uint8_t((v >> 16) & 0xFFu);
    b[o + 3] = std::uint8_t((v >> 24) & 0xFFu);
}

static std::vector<std::uint8_t> MakeRawChunk(const char rawId[4], std::size_t payloadSize)
{
    std::vector<std::uint8_t> out(8 + payloadSize, 0);
    std::memcpy(out.data(), rawId, 4);
    WriteLe32(out, 4, static_cast<std::uint32_t>(payloadSize));
    return out;
}

static LiquidLayer MakeLayer(LiquidCategory category, float height)
{
    LiquidLayer l;
    l.category = category;
    l.width = 1;
    l.height = 1;
    l.visible = {true};
    l.vertices.resize(4);
    for (auto& v : l.vertices)
    {
        v.height = height;
        v.depth = 64;
    }
    return l;
}

int main()
{
    McnkTargetHeader h;
    h.flags = 0x8000; // raw source-side bit15 must never pass through implicitly
    h.ix = 3;
    h.iy = 5;
    h.nLayers = 2;
    h.nDoodadRefs = 4;
    h.areaId = 123;
    h.nMapObjRefs = 2;
    h.holes = 0x8001;
    h.z = 7.0f;
    h.x = 8.0f;
    h.y = 9.0f;

    auto water = MakeLayer(LiquidCategory::Water, 10.0f);
    auto ocean = MakeLayer(LiquidCategory::Ocean, 20.0f);
    const auto liquid = BuildLegacyMclqBlock({water, ocean});
    assert(liquid.lossless);
    assert(liquid.block.mcnkLiquidFlags == 0x0C);

    McnkSubchunks chunks;
    chunks.mcvt = MakeRawChunk("TVCM", 145 * 4);
    chunks.mcnr = MakeRawChunk("RNCM", 145 * 3);
    chunks.mcly = MakeRawChunk("YLCM", 2 * 16);
    chunks.mcrf = MakeRawChunk("FRCM", 6 * 4);
    chunks.mcsh = MakeRawChunk("HSCM", 512);
    chunks.mcal = MakeRawChunk("LACM", 2048);
    chunks.mccv = MakeRawChunk("VCCM", 145 * 4);
    chunks.mclq = liquid.block;

    const auto out = SerializeVanillaMcnk(h, chunks);
    assert(out.bytes.size() > 136);
    assert(std::memcmp(out.bytes.data(), "KNCM", 4) == 0);
    assert(ReadLe32(out.bytes.data() + 4) == out.bytes.size() - 8);

    const std::size_t ph = 8;
    const std::uint32_t flags = ReadLe32(out.bytes.data() + ph + 0);
    assert((flags & 0x3C) == 0x0C);
    assert((flags & (1u << 15)) == 0); // raw bit15 was cleared; semantic control is separate
    assert((flags & 0x01u) != 0);      // MCSH present
    assert((flags & 0x40u) != 0);      // MCCV present

    assert(ReadLe32(out.bytes.data() + ph + 20) == 136);
    assert(out.layout.offsMCLY == out.layout.offsMCNR + chunks.mcnr.size() + 13);
    for (std::size_t i = 0; i < 13; ++i)
        assert(out.bytes[out.layout.offsMCNR + chunks.mcnr.size() + i] == 0);

    assert(ReadLe32(out.bytes.data() + ph + 36) == out.layout.offsMCAL);
    assert(ReadLe32(out.bytes.data() + ph + 40) == chunks.mcal.size());
    assert(ReadLe32(out.bytes.data() + ph + 44) == out.layout.offsMCSH);
    assert(ReadLe32(out.bytes.data() + ph + 48) == 512); // MCSH payload, not full chunk
    assert(ReadLe32(out.bytes.data() + ph + 96) == out.layout.offsMCLQ);
    assert(ReadLe32(out.bytes.data() + ph + 100) == 1616);

    assert(std::memcmp(out.bytes.data() + out.layout.offsMCVT, "TVCM", 4) == 0);
    assert(std::memcmp(out.bytes.data() + out.layout.offsMCAL, "LACM", 4) == 0);
    assert(std::memcmp(out.bytes.data() + out.layout.offsMCLQ, "QLCM", 4) == 0);
    assert(ReadLe32(out.bytes.data() + out.layout.offsMCLQ + 4) == 0);

    h.fullAlphaShadowEdges = true;
    const auto fullEdges = SerializeVanillaMcnk(h, chunks);
    const std::uint32_t fullFlags = ReadLe32(fullEdges.bytes.data() + ph);
    assert((fullFlags & (1u << 15)) != 0);
}
