#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct RibbonConversionResult
{
    M2ArrayRef target{};
    std::uint32_t droppedUnknown1NonZero = 0;
};

RibbonConversionResult ConvertWotlkRibbons(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceRibbons,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences = {},
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence = {});

} // namespace turtle335::m2
