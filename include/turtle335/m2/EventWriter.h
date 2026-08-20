#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

// WotLK Event 36B -> Classic/Turtle Event 44B.
// Event timers are special: they contain Ranges + Times but no value Keys.
// Empty per-animation event groups therefore do not synthesize fake events.
M2ArrayRef ConvertWotlkEvents(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceEvents,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

} // namespace turtle335::m2
