#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct BoneConversionResult
{
    M2ArrayRef target{};
};

// WotLK 88-byte bone -> Classic/Turtle 108-byte bone.
//
// - source unknown int32 at +12 is dropped;
// - translation/scaling use generic constant-track semantics;
// - rotation expands compressed int16 quaternion keys to Classic float4;
// - empty bone scaling groups synthesize identity (1,1,1);
// - per-sequence external .anim payloads are resolved through
//   externalBySequence.
BoneConversionResult ConvertWotlkBones(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceBones,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

} // namespace turtle335::m2
