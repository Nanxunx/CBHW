#pragma once

#include "turtle335/adt/LegacyLiquid.h"

#include <array>
#include <cstdint>
#include <vector>

namespace turtle335::adt {

using LegacyMclqPayloadBytes = std::array<std::uint8_t, 804>;
using LegacyMclqChunkBytes = std::array<std::uint8_t, 812>;

LegacyMclqPayloadBytes SerializeLegacyMclqPayload(const LegacyMclq& mclq);
// Single-record canonical raw chunk: QLCM + uint32(0) + 804-byte record.
LegacyMclqChunkBytes SerializeLegacyMclqChunk(const LegacyMclq& mclq);
// Multi-record canonical raw chunk. Records are written in fixed River/Ocean/Magma/Slime order.
std::vector<std::uint8_t> SerializeLegacyMclqBlock(const LegacyMclqBlock& block);

} // namespace turtle335::adt
