#include "turtle335/adt/WotlkAdtProbe.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace turtle335::adt;

static void WriteLe16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

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
    std::memcpy(&bits, &value, sizeof(bits));
    WriteLe32(bytes, offset, bits);
}

static std::vector<std::uint8_t> RawChunk(const char id[4], std::size_t payload)
{
    std::vector<std::uint8_t> out(8u + payload, 0);
    std::memcpy(out.data(), id, 4);
    WriteLe32(out, 4, static_cast<std::uint32_t>(payload));
    return out;
}

static std::vector<std::uint8_t> Mcly(std::uint32_t layers, std::uint32_t secondFlags = 0)
{
    std::vector<std::uint8_t> out = RawChunk("YLCM", static_cast<std::size_t>(layers) * 16u);
    if (layers > 1)
    {
        WriteLe32(out, 8u + 16u + 0u, 1); // texture id
        WriteLe32(out, 8u + 16u + 4u, secondFlags);
        WriteLe32(out, 8u + 16u + 8u, 0); // alpha offset
    }
    return out;
}

static std::vector<std::uint8_t> RleZeroAlpha()
{
    std::vector<std::uint8_t> payload;
    std::size_t remaining = 4096;
    while (remaining != 0)
    {
        const std::uint8_t count = static_cast<std::uint8_t>(remaining > 127 ? 127 : remaining);
        payload.push_back(static_cast<std::uint8_t>(0x80u | count));
        payload.push_back(0);
        remaining -= count;
    }
    std::vector<std::uint8_t> out = RawChunk("LACM", payload.size());
    std::memcpy(out.data() + 8u, payload.data(), payload.size());
    return out;
}

static std::vector<std::uint8_t> MinimalMh2o()
{
    constexpr std::size_t headerTable = 256u * 12u;
    constexpr std::size_t instanceOffset = headerTable;
    constexpr std::size_t payloadSize = instanceOffset + 24u;
    std::vector<std::uint8_t> out = RawChunk("O2HM", payloadSize);
    const std::size_t base = 8;

    WriteLe32(out, base + 0, static_cast<std::uint32_t>(instanceOffset));
    WriteLe32(out, base + 4, 1);
    WriteLe16(out, base + instanceOffset + 0, 7); // source LiquidType ID
    WriteLe16(out, base + instanceOffset + 2, 0); // HeightDepth
    WriteLeF32(out, base + instanceOffset + 4, 10.0f);
    WriteLeF32(out, base + instanceOffset + 8, 10.0f);
    out[base + instanceOffset + 12] = 0;
    out[base + instanceOffset + 13] = 0;
    out[base + instanceOffset + 14] = 1;
    out[base + instanceOffset + 15] = 1;
    // existsOffset=0 means full rectangle; vertexDataOffset=0 means flat minHeight.
    return out;
}

int main()
{
    WotlkAdtDocument source;
    source.version = 18;
    source.textures = {"Tileset\\Base.blp", "Tileset\\Detail.blp"};
    source.m2Placements.resize(1);
    source.wmoPlacements.resize(1);

    for (std::size_t slot = 0; slot < source.cells.size(); ++slot)
    {
        WotlkMcnkRecord& cell = source.cells[slot];
        cell.header.ix = static_cast<std::uint32_t>(slot % 16u);
        cell.header.iy = static_cast<std::uint32_t>(slot / 16u);
        cell.header.nLayers = 1;
        cell.mcly = Mcly(1);
    }

    source.cells[0].header.nLayers = 2;
    source.cells[0].mcly = Mcly(2, 0x100u);
    source.cells[0].mcal = RawChunk("LACM", 2048u);

    source.cells[1].header.nLayers = 2;
    source.cells[1].mcly = Mcly(2, 0x100u);
    source.cells[1].mcal = RawChunk("LACM", 4096u);

    source.cells[2].header.nLayers = 2;
    source.cells[2].mcly = Mcly(2, 0x100u | 0x200u);
    source.cells[2].mcal = RleZeroAlpha();

    source.cells[3].mcsh = RawChunk("HSCM", 512u);
    source.cells[4].mccv = RawChunk("VCCM", 145u * 4u);
    source.cells[5].mcse = RawChunk("ESCM", 28u);
    source.cells[5].header.nSndEmitters = 1;

    source.cells[6].header.flags |= (1u << 16);
    source.cells[6].header.legacy3E = 9;
    source.cells[6].header.disableDoodadsMap[0] = 1;
    source.cells[6].header.unused1 = 1;

    source.mh2o = MinimalMh2o();

    const WotlkAdtProbeReport report = ProbeWotlkAdt(source, false);
    assert(report.version == 18);
    assert(report.textureCount == 2);
    assert(report.m2PlacementCount == 1);
    assert(report.wmoPlacementCount == 1);
    assert(report.hasMh2o);
    assert(report.mh2oParsed);
    assert(report.liquidTypes.size() == 1);
    assert(report.liquidTypes[0].sourceLiquidType == 7);
    assert(report.liquidTypes[0].layerCount == 1);
    assert(report.liquidTypes[0].cellCount == 1);

    assert(report.cellsLegacy4Alpha == 1);
    assert(report.cellsBig8Alpha == 1);
    assert(report.cellsRle8Alpha == 1);
    assert(report.cellsNoAlpha == 253);
    assert(report.cellsMixedAlpha == 0);
    assert(report.cellsUnknownAlpha == 0);

    assert(report.cellsWithMcsh == 1);
    assert(report.cellsWithMccv == 1);
    assert(report.cellsWithMcse == 1);
    assert(report.soundEmitterCount == 1);
    assert(report.cellsHighResolutionHoles == 1);
    assert(report.cellsNonzeroField3E == 1);
    assert(report.cellsDisableDoodadsMap == 1);
    assert(report.cellsNonzeroTailDwords == 1);

    bool sawHighResBlocker = false;
    bool sawMccvRisk = false;
    bool sawWdtMismatch = false;
    for (const WotlkAdtProbeIssue& issue : report.issues)
    {
        if (issue.code == "HighResolutionHoles" && issue.blocker)
            sawHighResBlocker = true;
        if (issue.code == "TargetMccvLoss" && !issue.blocker)
            sawMccvRisk = true;
        if (issue.code == "WdtLegacyAlphaMismatch" && !issue.blocker)
            sawWdtMismatch = true;
    }
    assert(sawHighResBlocker);
    assert(sawMccvRisk);
    assert(sawWdtMismatch); // cell1/cell2 use big/RLE while supplied WDT hint is legacy.

    std::cout << "turtle335_wotlk_adt_probe_tests: OK\n";
    return 0;
}
