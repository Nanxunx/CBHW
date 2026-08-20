#pragma once

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <vector>

namespace turtle335::m2 {

// Canonical fixed-record conversions used by the whole-M2 writer.
// All functions resolve WotLK external .anim payloads and serialize Classic
// 28-byte Range/Times/Keys animation blocks.

M2ArrayRef ConvertWotlkColors(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceColors,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

M2ArrayRef ConvertWotlkTransparency(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceTransparency,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

M2ArrayRef ConvertWotlkTextureAnimations(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceTextureAnimations,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

M2ArrayRef ConvertWotlkAttachments(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    M2ArrayRef sourceAttachments,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

} // namespace turtle335::m2
