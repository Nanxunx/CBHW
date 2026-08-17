#pragma once

#include "turtle335/adt/LegacyLiquid.h"

#include <array>
#include <cstdint>

namespace turtle335::adt {

using LegacyMclqPayloadBytes = std::array<std::uint8_t, 804>;
using LegacyMclqChunkBytes = std::array<std::uint8_t, 812>;

LegacyMclqPayloadBytes SerializeLegacyMclqPayload(const LegacyMclq& mclq);
// Returns raw ADT bytes; logical MCLQ is stored on disk as FourCC QLCM.
LegacyMclqChunkBytes SerializeLegacyMclqChunk(const LegacyMclq& mclq);

} // namespace turtle335::adt
