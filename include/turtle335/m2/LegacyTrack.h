#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace turtle335::m2 {

struct WotlkTrackData
{
    std::int16_t interpolationType = 0;
    std::int16_t globalSequence = -1;
    std::vector<std::vector<std::uint32_t>> timestamps;
    std::vector<std::vector<std::vector<std::uint8_t>>> keys;
};

struct FlattenedLegacyTrack
{
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    std::vector<std::uint32_t> timestamps;
    std::vector<std::vector<std::uint8_t>> keys;
};

// Ribbon/Particle Golden targets treat one-model-sequence + one outer source
// group as a per-sequence track and synthesize Classic ranges/start-end keys.
// Generic Bone/Color/Transparency/TexAnim/Attachment/Light/Camera legacy
// converters instead preserve the special one-outer/one-key case as a
// constant track with no ranges. The writer must select the policy by block.
enum class LegacySingleKeyPolicy
{
    PerSequence,
    ConstantNoRanges,
};

WotlkTrackData ParseWotlkTrack(
    const std::vector<std::uint8_t>& source,
    std::size_t offset,
    std::size_t keySize);

FlattenedLegacyTrack FlattenLegacyValueTrack(
    const WotlkTrackData& track,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<std::uint8_t>& defaultKey,
    LegacySingleKeyPolicy singleKeyPolicy = LegacySingleKeyPolicy::PerSequence);

std::array<std::uint8_t, 28> SerializeClassicTrack(
    BinaryBuilder& builder,
    const WotlkTrackData& sourceTrack,
    const FlattenedLegacyTrack& track);

} // namespace turtle335::m2
