#include "turtle335/m2/LightWriter.h"

#include "turtle335/m2/ExternalAnim.h"
#include "turtle335/m2/LegacyTrack.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kSourceLightSize = 156u;
constexpr std::size_t kTargetLightSize = 212u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error("Light interpolation OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

void ValidateRecords(const std::vector<std::uint8_t>& source, const M2ArrayRef ref)
{
    if (ref.count == 0u)
        return;
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * kSourceLightSize;
    if (ref.offset == 0u || static_cast<std::size_t>(ref.offset) > source.size() ||
        bytes > source.size() - static_cast<std::size_t>(ref.offset))
        throw std::runtime_error("WotLK Light records OOB");
}

std::array<std::uint8_t, 28> ConvertTrack(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::size_t baseKeySize,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto interpolation = static_cast<std::int16_t>(ReadU16(source, trackOffset));
    const std::size_t multiplicity = (interpolation == 2 || interpolation == 3) ? 3u : 1u;
    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        baseKeySize * multiplicity,
        sequences,
        externalBySequence);
    const auto flattened = FlattenLegacyValueTrack(
        sourceTrack,
        windows,
        std::vector<std::uint8_t>(baseKeySize * multiplicity, 0u),
        LegacySingleKeyPolicy::ConstantNoRanges);
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

} // namespace

M2ArrayRef ConvertWotlkLights(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceLights,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    ValidateRecords(source, sourceLights);
    M2ArrayRef target{sourceLights.count, 0u};
    if (sourceLights.count == 0u)
        return target;

    target.offset = output.Reserve(static_cast<std::size_t>(sourceLights.count) * kTargetLightSize);
    for (std::uint32_t i = 0u; i < sourceLights.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceLights.offset) +
                               static_cast<std::size_t>(i) * kSourceLightSize;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(kTargetLightSize);
        std::array<std::uint8_t, kTargetLightSize> rec{};

        // ID, bone, position.
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(so), 16u, rec.begin());

        struct TrackSpec
        {
            std::size_t sourceOffset;
            std::size_t targetOffset;
            std::size_t baseKeySize;
        };
        constexpr std::array<TrackSpec, 7> specs{{
            {16u,  16u, 12u}, // ambient color Vec3
            {36u,  44u, 4u},  // ambient intensity
            {56u,  72u, 12u}, // diffuse color Vec3
            {76u, 100u, 4u},  // diffuse intensity
            {96u, 128u, 4u},  // attenuation start
            {116u,156u, 4u},  // attenuation end
            {136u,184u, 4u},  // visibility/unknown int-like track
        }};

        for (const auto& spec : specs)
        {
            const auto track = ConvertTrack(
                output,
                source,
                so + spec.sourceOffset,
                spec.baseKeySize,
                windows,
                sequences,
                externalBySequence);
            std::copy(track.begin(), track.end(), rec.begin() + static_cast<std::ptrdiff_t>(spec.targetOffset));
        }

        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

} // namespace turtle335::m2
