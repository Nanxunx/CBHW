#include "turtle335/m2/WotlkM2Reader.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kWotlkHeaderSize = 304u;
constexpr std::size_t kWotlkSequenceSize = 64u;
constexpr std::size_t kWotlkRibbonSize = 176u;
constexpr std::size_t kWotlkParticleSize = 476u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 2u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside M2 bytes");
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside M2 bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

M2ArrayRef ReadPair(const std::vector<std::uint8_t>& bytes, std::size_t offset, const char* what)
{
    return {ReadU32(bytes, offset, what), ReadU32(bytes, offset + 4u, what)};
}

void ValidateSpan(const std::vector<std::uint8_t>& bytes, const M2ArrayRef ref, const std::size_t stride, const char* what)
{
    if (ref.count == 0u)
        return;
    if (ref.offset == 0u)
        throw std::runtime_error(std::string(what) + " has non-zero count with zero offset");
    const std::size_t count = static_cast<std::size_t>(ref.count);
    if (stride != 0u && count > (bytes.size() / stride) + 1u)
        throw std::runtime_error(std::string(what) + " count is implausibly large");
    const std::size_t size = count * stride;
    const std::size_t offset = static_cast<std::size_t>(ref.offset);
    if (offset > bytes.size() || size > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " record array extends outside M2 bytes");
}

} // namespace

WotlkM2Document ParseWotlkM2(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < kWotlkHeaderSize)
        throw std::runtime_error("M2 is shorter than the WotLK MD20 header");
    if (std::memcmp(bytes.data(), "MD20", 4u) != 0)
        throw std::runtime_error("M2 magic is not MD20");

    WotlkM2Document model;
    model.version = ReadU32(bytes, 0x04u, "M2 version");
    if (model.version != 264u)
        throw std::runtime_error("WotLK M2 reader requires MD20 version 264");

    model.name = ReadPair(bytes, 0x08u, "name");
    model.globalFlags = ReadU32(bytes, 0x10u, "global flags");
    model.globalSequences = ReadPair(bytes, 0x14u, "global sequences");
    model.animations = ReadPair(bytes, 0x1cu, "animations");
    model.animationLookup = ReadPair(bytes, 0x24u, "animation lookup");
    model.bones = ReadPair(bytes, 0x2cu, "bones");
    model.keyBoneLookup = ReadPair(bytes, 0x34u, "key bone lookup");
    model.vertices = ReadPair(bytes, 0x3cu, "vertices");
    model.viewCount = ReadU32(bytes, 0x44u, "view count");
    model.colors = ReadPair(bytes, 0x48u, "colors");
    model.textures = ReadPair(bytes, 0x50u, "textures");
    model.transparency = ReadPair(bytes, 0x58u, "transparency");
    model.textureAnimations = ReadPair(bytes, 0x60u, "texture animations");
    model.attachments = ReadPair(bytes, 0xf0u, "attachments");
    model.attachmentLookup = ReadPair(bytes, 0xf8u, "attachment lookup");
    model.events = ReadPair(bytes, 0x100u, "events");
    model.lights = ReadPair(bytes, 0x108u, "lights");
    model.cameras = ReadPair(bytes, 0x110u, "cameras");
    model.cameraLookup = ReadPair(bytes, 0x118u, "camera lookup");
    model.ribbons = ReadPair(bytes, 0x120u, "ribbons");
    model.particles = ReadPair(bytes, 0x128u, "particles");

    ValidateSpan(bytes, model.animations, kWotlkSequenceSize, "animations");
    ValidateSpan(bytes, model.ribbons, kWotlkRibbonSize, "ribbons");
    ValidateSpan(bytes, model.particles, kWotlkParticleSize, "particles");

    if (model.name.count != 0u)
    {
        const std::size_t off = static_cast<std::size_t>(model.name.offset);
        const std::size_t size = static_cast<std::size_t>(model.name.count);
        if (off == 0u || off > bytes.size() || size > bytes.size() - off)
            throw std::runtime_error("M2 name extends outside file");
    }

    model.sequences.reserve(model.animations.count);
    std::unordered_map<std::uint16_t, std::size_t> animationIdCounts;
    for (std::uint32_t i = 0; i < model.animations.count; ++i)
    {
        const std::size_t offset = static_cast<std::size_t>(model.animations.offset) + static_cast<std::size_t>(i) * kWotlkSequenceSize;
        WotlkM2Sequence seq;
        seq.animationId = ReadU16(bytes, offset + 0u, "sequence AnimationID");
        seq.subAnimationId = ReadU16(bytes, offset + 2u, "sequence SubAnimationID");
        seq.length = ReadU32(bytes, offset + 4u, "sequence length");
        seq.flags = ReadU32(bytes, offset + 12u, "sequence flags");
        seq.index = ReadU16(bytes, offset + 62u, "sequence Index");
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 8u), seq.tailFromMoveSpeed.size(), seq.tailFromMoveSpeed.begin());
        model.sequences.push_back(seq);
        ++animationIdCounts[seq.animationId];
        model.hasAliasSequences = model.hasAliasSequences || ((seq.flags & 0x40u) != 0u);
        model.hasSubAnimations = model.hasSubAnimations || (seq.subAnimationId != 0u);
    }
    for (const auto& entry : animationIdCounts)
        model.hasDuplicateAnimationIds = model.hasDuplicateAnimationIds || entry.second > 1u;
    return model;
}

M2FeatureReport InspectM2Features(const WotlkM2Document& model)
{
    M2FeatureReport report;
    report.animated = model.animations.count != 0u;
    report.textureAnimated = model.textureAnimations.count != 0u;
    report.hasEvents = model.events.count != 0u;
    report.hasRibbons = model.ribbons.count != 0u;
    report.hasParticles = model.particles.count != 0u;
    report.hasAliasSequences = model.hasAliasSequences;
    report.hasSubAnimations = model.hasSubAnimations;
    report.hasDuplicateAnimationIds = model.hasDuplicateAnimationIds;
    return report;
}

M2ConversionGate ClassifyM2(const WotlkM2Document& model, const bool hasExternalAnimSidecar)
{
    if (model.ribbons.count != 0u) return M2ConversionGate::RibbonWriterRequired;
    if (model.particles.count != 0u) return M2ConversionGate::ParticleWriterRequired;
    if (hasExternalAnimSidecar) return M2ConversionGate::ExternalAnimCopy;
    if (model.textureAnimations.count != 0u) return M2ConversionGate::TextureAnimationGolden;
    if (model.hasAliasSequences || model.hasSubAnimations || model.hasDuplicateAnimationIds) return M2ConversionGate::AnimationHighRisk;
    if (model.animations.count != 0u) return M2ConversionGate::AnimationBaseline;
    return M2ConversionGate::StaticGeometry;
}

const char* ToString(const M2ConversionGate gate)
{
    switch (gate)
    {
    case M2ConversionGate::StaticGeometry: return "STATIC_GEOMETRY";
    case M2ConversionGate::AnimationBaseline: return "ANIMATION_BASELINE";
    case M2ConversionGate::AnimationHighRisk: return "ANIMATION_HIGH_RISK";
    case M2ConversionGate::TextureAnimationGolden: return "TEXANIM_GOLDEN";
    case M2ConversionGate::ExternalAnimCopy: return "EXTERNAL_ANIM_COPY";
    case M2ConversionGate::ParticleWriterRequired: return "PARTICLE_WRITER_REQUIRED";
    case M2ConversionGate::RibbonWriterRequired: return "RIBBON_WRITER_REQUIRED";
    }
    return "UNKNOWN";
}

} // namespace turtle335::m2
