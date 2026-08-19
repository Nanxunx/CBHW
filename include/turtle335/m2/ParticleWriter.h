#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct ParticleConversionResult
{
    M2ArrayRef target{};
    std::uint32_t convertedEmitters = 0;
};

// Golden-gated WotLK v264 (476-byte) ParticleEmitter -> Classic/Turtle v256
// (504-byte) conversion. Emitters with non-zero unknown-reference arrays are
// rejected until a successful target Golden covers that payload class.
ParticleConversionResult ConvertWotlkParticles(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceParticles,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences = {},
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence = {});

} // namespace turtle335::m2
