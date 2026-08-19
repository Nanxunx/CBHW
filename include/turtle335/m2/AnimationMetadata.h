#pragma once

#include "turtle335/m2/WotlkM2Reader.h"

#include <array>
#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct ClassicSequenceWindow
{
    std::uint32_t start = 0;
    std::uint32_t end = 0;
};

struct PlayableAnimationRecord
{
    std::int16_t fallbackAnimationId = 0;
    std::int16_t flags = 0;
};

using AnimationFallbackGraph = std::array<std::uint16_t, 226>;

std::vector<ClassicSequenceWindow> BuildClassicSequenceWindows(
    const std::vector<WotlkM2Sequence>& sequences,
    std::uint32_t gap = 3333u);

// Emits 68-byte Classic sequence records and preserves the source bytes from
// moveSpeed through Index exactly, including source Sequence.Index semantics.
std::vector<std::uint8_t> BuildClassicSequenceRecords(
    const std::vector<WotlkM2Sequence>& sequences,
    std::uint32_t gap = 3333u);

std::vector<std::int16_t> BuildClassicAnimationLookup(
    const std::vector<WotlkM2Sequence>& sequences);

AnimationFallbackGraph ParseBuild12340AnimationFallbackGraph(
    const std::vector<std::uint8_t>& dbcBytes);

std::vector<PlayableAnimationRecord> BuildClassicPlayableAnimationLookup(
    const std::vector<WotlkM2Sequence>& sequences,
    const AnimationFallbackGraph& graph);

} // namespace turtle335::m2
