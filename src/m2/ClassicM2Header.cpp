#include "turtle335/m2/ClassicM2Header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace turtle335::m2 {
namespace {

void PutU32(std::array<std::uint8_t, kClassicM2HeaderSize>& out, const std::size_t o,
            const std::uint32_t v)
{
    out[o] = static_cast<std::uint8_t>(v & 0xffu);
    out[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
    out[o + 2] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
    out[o + 3] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
}

void PutRef(std::array<std::uint8_t, kClassicM2HeaderSize>& out, const std::size_t o,
            const M2ArrayRef ref)
{
    PutU32(out, o, ref.count);
    PutU32(out, o + 4u, ref.offset);
}

} // namespace

std::uint32_t CanonicalizeClassicGlobalFlags(const std::uint32_t sourceGlobalFlags)
{
    // WotLK flag 0x8 enables the optional texture-combiner pair appended after
    // the v264 base header. Successful Classic/Turtle Golden targets do not
    // carry that extension; v256 material lookups are serialized explicitly.
    return sourceGlobalFlags & ~0x8u;
}

std::array<std::uint8_t, kClassicM2HeaderSize> BuildClassicM2Header(
    const ClassicM2HeaderFields& f)
{
    std::array<std::uint8_t, kClassicM2HeaderSize> out{};
    std::memcpy(out.data(), "MD20", 4u);
    PutU32(out, 0x004u, 256u);
    PutU32(out, 0x008u, f.nameLength);
    PutU32(out, 0x00cu, f.nameOffset);
    PutU32(out, 0x010u, f.globalFlags);

    PutRef(out, 0x014u, f.globalSequences);
    PutRef(out, 0x01cu, f.animations);
    PutRef(out, 0x024u, f.animationLookup);
    PutRef(out, 0x02cu, f.playableAnimationLookup);
    PutRef(out, 0x034u, f.bones);
    PutRef(out, 0x03cu, f.keyBoneLookup);
    PutRef(out, 0x044u, f.vertices);
    PutRef(out, 0x04cu, f.views);
    PutRef(out, 0x054u, f.colors);
    PutRef(out, 0x05cu, f.textures);
    PutRef(out, 0x064u, f.transparency);
    PutRef(out, 0x06cu, f.unknownI);
    PutRef(out, 0x074u, f.textureAnimations);
    PutRef(out, 0x07cu, f.textureReplace);
    PutRef(out, 0x084u, f.renderFlags);
    PutRef(out, 0x08cu, f.boneLookup);
    PutRef(out, 0x094u, f.textureLookup);
    PutRef(out, 0x09cu, f.textureUnitLookup);
    PutRef(out, 0x0a4u, f.transparencyLookup);
    PutRef(out, 0x0acu, f.textureAnimationLookup);

    std::copy(
        f.boundsAndCollisionFloats.begin(),
        f.boundsAndCollisionFloats.end(),
        out.begin() + 0x0b4u);

    PutRef(out, 0x0ecu, f.boundingTriangles);
    PutRef(out, 0x0f4u, f.boundingVertices);
    PutRef(out, 0x0fcu, f.boundingNormals);
    PutRef(out, 0x104u, f.attachments);
    PutRef(out, 0x10cu, f.attachmentLookup);
    PutRef(out, 0x114u, f.events);
    PutRef(out, 0x11cu, f.lights);
    PutRef(out, 0x124u, f.cameras);
    PutRef(out, 0x12cu, f.cameraLookup);
    PutRef(out, 0x134u, f.ribbons);
    PutRef(out, 0x13cu, f.particles);
    return out;
}

} // namespace turtle335::m2
