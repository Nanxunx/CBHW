#pragma once

#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct ClassicM2WriteOptions
{
    // Light layout is reference-validated by Wallcraft/Coffee but the current
    // selected paired corpus has no non-zero Light Golden. Production output
    // therefore blocks it by default until a tiny targeted pair is checked.
    bool allowReferenceGatedLights = false;
    bool runStrictValidator = true;
};

struct ClassicM2WriteResult
{
    std::vector<std::uint8_t> bytes;
    std::uint32_t droppedRibbonUnknown1NonZero = 0;
};

// Assemble one canonical Turtle/Classic MD20 v256 from a WotLK v264 M2.
//
// Inputs:
// - sourceM2: raw v264 M2 bytes
// - skinFiles: external WotLK .skin files in view order
// - animationDataDbc: build12340 AnimationData.dbc used to build canonical
//   226-entry PlayableAnimationLookup
// - externalBySequence: optional raw .anim sidecar bytes aligned to source
//   sequence order; nullptr means no sidecar supplied for that sequence
//
// Unsupported/unknown semantic classes fail closed rather than silently
// emitting lossy data.
ClassicM2WriteResult ConvertWotlkM2ToClassic(
    const std::vector<std::uint8_t>& sourceM2,
    const std::vector<std::vector<std::uint8_t>>& skinFiles,
    const std::vector<std::uint8_t>& animationDataDbc,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence = {},
    const ClassicM2WriteOptions& options = {});

} // namespace turtle335::m2
