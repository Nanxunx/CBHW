#pragma once

#include "turtle335/m2/AnimationMetadata.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace turtle335::m2 {

struct WotlkTrackData
{
    std::uint16_t interpolation = 0;
    std::int16_t globalSequence = -1;
    std::size_t keySize = 0;
    std::vector<std::vector<std::uint32_t>> times;
    std::vector<std::vector<std::vector<std::uint8_t>>> values;
};

struct ClassicTrackData
{
    std::uint16_t interpolation = 0;
    std::int16_t globalSequence = -1;
    std::size_t keySize = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    std::vector<std::uint32_t> times;
    std::vector<std::vector<std::uint8_t>> values;
};

enum class EmptyOuterMode
{
    Empty,
    EnabledOne,
};

WotlkTrackData ParseWotlkTrack(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::size_t baseKeySize);

ClassicTrackData ConvertLegacyTrack(
    const WotlkTrackData& source,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<std::uint8_t>& baseDefault,
    EmptyOuterMode emptyMode = EmptyOuterMode::Empty);

class AbsoluteBlockBuilder
{
public:
    AbsoluteBlockBuilder(std::size_t fixedSize, std::uint32_t absoluteOffset);

    std::vector<std::uint8_t>& Fixed();
    const std::vector<std::uint8_t>& Fixed() const;

    std::uint32_t Append(const std::vector<std::uint8_t>& bytes, std::size_t alignment = 4u);
    std::vector<std::uint8_t> Finish() const;

private:
    std::vector<std::uint8_t> fixed_;
    std::vector<std::uint8_t> payload_;
    std::uint32_t absoluteOffset_ = 0;
};

void SerializeClassicTrack(
    AbsoluteBlockBuilder& builder,
    std::size_t fixedOffset,
    const ClassicTrackData& track);

} // namespace turtle335::m2
