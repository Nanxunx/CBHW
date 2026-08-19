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

WotlkTrackData ParseWotlkTrack(const std::vector<std::uint8_t>& source, std::size_t offset, std::size_t keySize);
FlattenedLegacyTrack FlattenLegacyValueTrack(const WotlkTrackData& track, const std::vector<ClassicSequenceWindow>& windows, const std::vector<std::uint8_t>& defaultKey);
std::array<std::uint8_t, 28> SerializeClassicTrack(BinaryBuilder& builder, const WotlkTrackData& sourceTrack, const FlattenedLegacyTrack& track);

} // namespace turtle335::m2
