#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace turtle335::adt {

struct WdtTileEntry
{
    std::uint32_t flags = 0;
    std::uint32_t asyncId = 0;
};

struct WdtWriterInput
{
    // MPHD payload is 8 uint32 values. This writer currently supports terrain
    // WDTs only; global-WMO flag bit0 in mphd[0] is rejected.
    std::array<std::uint32_t, 8> mphd{};
    std::array<WdtTileEntry, 64 * 64> tiles{};
};

struct WdtValidationReport
{
    std::size_t fileSize = 0;
    std::uint32_t version = 0;
    std::uint32_t headerFlags = 0;
    std::size_t presentTiles = 0;
};

constexpr std::size_t WdtTileSlot(std::uint32_t x, std::uint32_t y)
{
    return static_cast<std::size_t>(y) * 64u + static_cast<std::size_t>(x);
}

void SetWdtTerrainTile(WdtWriterInput& input,
                       std::uint32_t x,
                       std::uint32_t y,
                       bool present);

std::vector<std::uint8_t> SerializeTerrainWdt(const WdtWriterInput& input);
WdtValidationReport ValidateTerrainWdt(const std::vector<std::uint8_t>& bytes);

} // namespace turtle335::adt
