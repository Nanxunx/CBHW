#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace turtle335::adt {

struct AdtVec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct M2PlacementInput
{
    std::string assetPath;
    std::uint32_t uniqueId = 0;
    AdtVec3 position;
    AdtVec3 rotation;
    std::uint16_t scale = 1024;
    std::uint16_t flags = 0;
};

struct WmoPlacementInput
{
    std::string assetPath;
    std::uint32_t uniqueId = 0;
    AdtVec3 position;
    AdtVec3 rotation;
    AdtVec3 minimumExtent;
    AdtVec3 maximumExtent;
    std::uint16_t flags = 0;
    std::uint16_t doodadSet = 0;
    std::uint16_t nameSet = 0;
    std::uint16_t scale = 1024;
};

struct PlacementTables
{
    // Complete raw ADT top-level chunks, using reversed on-disk FourCCs.
    // Empty when the corresponding placement family is absent.
    std::vector<std::uint8_t> mmdx;
    std::vector<std::uint8_t> mmid;
    std::vector<std::uint8_t> mwmo;
    std::vector<std::uint8_t> mwid;
    std::vector<std::uint8_t> mddf;
    std::vector<std::uint8_t> modf;

    // One NameId per source placement, in placement-table order.
    std::vector<std::uint32_t> m2NameIds;
    std::vector<std::uint32_t> wmoNameIds;
};

PlacementTables BuildPlacementTables(const std::vector<M2PlacementInput>& m2Placements,
                                     const std::vector<WmoPlacementInput>& wmoPlacements);

// Builds a complete raw MCRF chunk (FRCM + size + payload).
// M2 references are serialized first, then WMO references, matching MCNK's
// nDoodadRefs / nMapObjRefs split contract. Returns empty when both lists are empty.
std::vector<std::uint8_t> BuildMcrfChunk(const std::vector<std::uint32_t>& m2Refs,
                                         const std::vector<std::uint32_t>& wmoRefs,
                                         std::size_t m2PlacementCount,
                                         std::size_t wmoPlacementCount);

} // namespace turtle335::adt
