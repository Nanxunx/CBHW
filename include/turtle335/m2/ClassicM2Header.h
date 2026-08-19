#pragma once

#include "turtle335/m2/WotlkM2Reader.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace turtle335::m2 {

constexpr std::size_t kClassicM2HeaderSize = 324u;

struct ClassicM2HeaderFields
{
    std::uint32_t nameLength = 0;
    std::uint32_t nameOffset = 0;
    std::uint32_t globalFlags = 0;

    M2ArrayRef globalSequences{};
    M2ArrayRef animations{};
    M2ArrayRef animationLookup{};
    M2ArrayRef playableAnimationLookup{};
    M2ArrayRef bones{};
    M2ArrayRef keyBoneLookup{};
    M2ArrayRef vertices{};
    M2ArrayRef views{};
    M2ArrayRef colors{};
    M2ArrayRef textures{};
    M2ArrayRef transparency{};
    M2ArrayRef unknownI{};
    M2ArrayRef textureAnimations{};
    M2ArrayRef textureReplace{};
    M2ArrayRef renderFlags{};
    M2ArrayRef boneLookup{};
    M2ArrayRef textureLookup{};
    M2ArrayRef textureUnitLookup{};
    M2ArrayRef transparencyLookup{};
    M2ArrayRef textureAnimationLookup{};

    std::array<std::uint8_t, 56> boundsAndCollisionFloats{};

    M2ArrayRef boundingTriangles{};
    M2ArrayRef boundingVertices{};
    M2ArrayRef boundingNormals{};
    M2ArrayRef attachments{};
    M2ArrayRef attachmentLookup{};
    M2ArrayRef events{};
    M2ArrayRef lights{};
    M2ArrayRef cameras{};
    M2ArrayRef cameraLookup{};
    M2ArrayRef ribbons{};
    M2ArrayRef particles{};
};

std::array<std::uint8_t, kClassicM2HeaderSize> BuildClassicM2Header(
    const ClassicM2HeaderFields& fields);

std::uint32_t CanonicalizeClassicGlobalFlags(std::uint32_t sourceGlobalFlags);

} // namespace turtle335::m2
