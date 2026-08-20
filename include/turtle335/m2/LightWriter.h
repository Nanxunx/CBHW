#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

// WotLK Light 156B -> Classic/Turtle Light 212B.
// Layout is independently corroborated by Wallcraft M2 Workshop and the
// historical LKBC structures. Unlike Camera/Ribbon/Particle, the currently
// packaged selected Golden corpus has no non-zero Light sample, so the whole
// writer should keep Light-bearing output behind a validation gate until a
// targeted successful pair is checked.
M2ArrayRef ConvertWotlkLights(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceLights,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

} // namespace turtle335::m2
