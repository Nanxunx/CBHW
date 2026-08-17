#pragma once

#include "turtle335/adt/McnkWriter.h"
#include "turtle335/adt/PlacementWriter.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace turtle335::adt {

struct AdtCellInput
{
    McnkTargetHeader header;
    McnkSubchunks subchunks;
    std::vector<std::uint32_t> m2Refs;
    std::vector<std::uint32_t> wmoRefs;
};

struct AdtWriterInput
{
    // MTEX order is semantic: MCLY textureId indexes this exact sequence.
    std::vector<std::string> textures;
    std::vector<M2PlacementInput> m2Placements;
    std::vector<WmoPlacementInput> wmoPlacements;
    std::array<AdtCellInput, 256> cells;

    // Optional complete raw MFBO chunk (OBFM + size + payload).
    std::vector<std::uint8_t> mfbo;
};

struct AdtLayout
{
    std::uint32_t mverOffset = 0;
    std::uint32_t mhdrOffset = 0;
    std::uint32_t mcinOffset = 0;
    std::uint32_t mtexOffset = 0;
    std::uint32_t mmdxOffset = 0;
    std::uint32_t mmidOffset = 0;
    std::uint32_t mwmoOffset = 0;
    std::uint32_t mwidOffset = 0;
    std::uint32_t mddfOffset = 0;
    std::uint32_t modfOffset = 0;
    std::uint32_t mfboOffset = 0;
    std::array<std::uint32_t, 256> mcnkOffsets{};
    std::array<std::uint32_t, 256> mcnkSizes{};
};

struct SerializedAdt
{
    std::vector<std::uint8_t> bytes;
    AdtLayout layout;
    PlacementTables placements;
};

// Canonical Vanilla/Turtle ADT writer.
// Root order is intentionally fixed to MVER(12) + MHDR(72) + MCIN because
// Penqle/tortoise-wow's legacy loader assumes MCIN begins at byte 84.
SerializedAdt SerializeVanillaAdt(const AdtWriterInput& input);

// Throws std::exception on structural/root-pointer inconsistency.
void ValidateVanillaAdtRoot(const std::vector<std::uint8_t>& bytes);

} // namespace turtle335::adt
