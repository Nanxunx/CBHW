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

static LegacyMclq MakeWater()
{
    LiquidLayer l;
    l.category = LiquidCategory::Water;
    l.width = 1;
    l.height = 1;
    l.visible = {true};
    l.vertices.resize(4);
    for (auto& v : l.vertices)
    {
        v.height = 10.0f;
        v.depth = 64;
    }
    auto built = BuildLegacyMclq({l});
    assert(built.mclq.has_value());
    return *built.mclq;
}

int main()
{
    McnkTargetHeader h;
    h.flags = 0x100;
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

    McnkSubchunks chunks;
    chunks.mcvt = MakeRawChunk("TVCM", 145 * 4);
    chunks.mcnr = MakeRawChunk("RNCM", 145 * 3);
    chunks.mcly = MakeRawChunk("YLCM", 2 * 16);
    chunks.mcrf = MakeRawChunk("FRCM", 6 * 4);
    chunks.mcsh = MakeRawChunk("HSCM", 512);
    chunks.mcal = MakeRawChunk("LACM", 2048);
    chunks.mclq = MakeWater();

    const auto out = SerializeVanillaMcnk(h, chunks);
    assert(out.bytes.size() > 136);
    assert(std::memcmp(out.bytes.data(), "KNCM", 4) == 0);
    assert(ReadLe32(out.bytes.data() + 4) == out.bytes.size() - 8);

    const std::size_t ph = 8;
    const std::uint32_t flags = ReadLe32(out.bytes.data() + ph + 0);
    assert((flags & 0x04) != 0);
    assert((flags & (1u << 15)) != 0);
    assert(ReadLe32(out.bytes.data() + ph + 20) == 136);
    assert(ReadLe32(out.bytes.data() + ph + 36) == out.layout.offsMCAL);
    assert(ReadLe32(out.bytes.data() + ph + 40) == chunks.mcal.size());
    assert(ReadLe32(out.bytes.data() + ph + 44) == out.layout.offsMCSH);
    assert(ReadLe32(out.bytes.data() + ph + 48) == chunks.mcsh.size());
    assert(ReadLe32(out.bytes.data() + ph + 96) == out.layout.offsMCLQ);
    assert(ReadLe32(out.bytes.data() + ph + 100) == 812);

    assert(std::memcmp(out.bytes.data() + out.layout.offsMCVT, "TVCM", 4) == 0);
    assert(std::memcmp(out.bytes.data() + out.layout.offsMCAL, "LACM", 4) == 0);
    assert(std::memcmp(out.bytes.data() + out.layout.offsMCLQ, "QLCM", 4) == 0);
}
