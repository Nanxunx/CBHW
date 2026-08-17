#pragma once

#include "turtle335/adt/LegacyLiquid.h"

#include <array>
#include <cstdint>

namespace turtle335::adt {

using LegacyMclqPayloadBytes = std::array<std::uint8_t, 804>;
using LegacyMclqChunkBytes = std::array<std::uint8_t, 812>;

LegacyMclqPayloadBytes SerializeLegacyMclqPayload(const LegacyMclq& mclq);
LegacyMclqChunkBytes SerializeLegacyMclqChunk(const LegacyMclq& mclq);

} // namespace turtle335::adt
