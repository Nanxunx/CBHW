#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace turtle335::m2 {

struct M2ArrayRef
{
    std::uint32_t count = 0;
    std::uint32_t offset = 0;
};

struct WotlkM2Sequence
{
    std::uint16_t animationId = 0;
    std::uint16_t subAnimationId = 0;
    std::uint32_t length = 0;
    std::uint32_t flags = 0;
    std::uint16_t index = 0;
    std::array<std::uint8_t, 56> tailFromMoveSpeed{};
};

struct WotlkM2Document
{
    std::uint32_t version = 0;
    std::uint32_t globalFlags = 0;
    M2ArrayRef name{};
    M2ArrayRef globalSequences{};
    M2ArrayRef animations{};
    M2ArrayRef animationLookup{};
    M2ArrayRef bones{};
    M2ArrayRef keyBoneLookup{};
    M2ArrayRef vertices{};
    std::uint32_t viewCount = 0;
    M2ArrayRef colors{};
    M2ArrayRef textures{};
    M2ArrayRef transparency{};
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
    std::vector<WotlkM2Sequence> sequences;
    bool hasAliasSequences = false;
    bool hasSubAnimations = false;
    bool hasDuplicateAnimationIds = false;
};

enum class M2ConversionGate
{
    StaticGeometry,
    AnimationBaseline,
    AnimationHighRisk,
    TextureAnimationGolden,
    ExternalAnimCopy,
    ParticleWriterRequired,
    RibbonWriterRequired,
};

struct M2FeatureReport
{
    bool animated = false;
    bool textureAnimated = false;
    bool hasEvents = false;
    bool hasRibbons = false;
    bool hasParticles = false;
    bool hasAliasSequences = false;
    bool hasSubAnimations = false;
    bool hasDuplicateAnimationIds = false;
};

// Parse an MD20 v264 build-12340 model. The full fixed 304-byte WotLK header
// is exposed so the canonical whole-M2 writer can copy/relocate every legacy
// array it supports. Sequence/ribbon/particle fixed record arrays are bounds
// checked here; feature-specific payload checks remain in their writers.
WotlkM2Document ParseWotlkM2(const std::vector<std::uint8_t>& bytes);
M2FeatureReport InspectM2Features(const WotlkM2Document& model);
M2ConversionGate ClassifyM2(const WotlkM2Document& model, bool hasExternalAnimSidecar = false);
const char* ToString(M2ConversionGate gate);

} // namespace turtle335::m2
