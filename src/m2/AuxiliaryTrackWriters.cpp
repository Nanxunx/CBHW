#include "turtle335/m2/AuxiliaryTrackWriters.h"

#include "turtle335/m2/ExternalAnim.h"
#include "turtle335/m2/LegacyTrack.h"
#include "turtle335/m2/QuaternionCodec.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace turtle335::m2 {
namespace {

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error("auxiliary track interpolation OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

void ValidateRecords(
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef ref,
    const std::size_t stride,
    const char* what)
{
    if (ref.count == 0u)
        return;
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * stride;
    if (ref.offset == 0u || static_cast<std::size_t>(ref.offset) > source.size() ||
        bytes > source.size() - static_cast<std::size_t>(ref.offset))
        throw std::runtime_error(std::string(what) + " records OOB");
}

std::array<std::uint8_t, 28> ConvertRawValueTrack(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::size_t baseKeySize,
    const std::vector<std::uint8_t>& baseDefault,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto interpolation = static_cast<std::int16_t>(ReadU16(source, trackOffset));
    const std::size_t multiplicity = (interpolation == 2 || interpolation == 3) ? 3u : 1u;
    std::vector<std::uint8_t> defaultKey;
    defaultKey.reserve(baseDefault.size() * multiplicity);
    for (std::size_t i = 0u; i < multiplicity; ++i)
        defaultKey.insert(defaultKey.end(), baseDefault.begin(), baseDefault.end());

    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        baseKeySize * multiplicity,
        sequences,
        externalBySequence);
    const auto flattened = FlattenLegacyValueTrack(
        sourceTrack,
        windows,
        defaultKey,
        LegacySingleKeyPolicy::ConstantNoRanges);
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

std::array<std::uint8_t, 28> ConvertQuaternionTrack(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto interpolation = static_cast<std::int16_t>(ReadU16(source, trackOffset));
    const std::size_t multiplicity = (interpolation == 2 || interpolation == 3) ? 3u : 1u;

    std::vector<std::uint8_t> compressedIdentity;
    compressedIdentity.reserve(8u * multiplicity);
    constexpr std::array<std::int16_t, 4> identity{{-32767, -32767, -32767, -1}};
    for (std::size_t n = 0u; n < multiplicity; ++n)
    {
        for (const auto value : identity)
        {
            const auto u = static_cast<std::uint16_t>(value);
            compressedIdentity.push_back(static_cast<std::uint8_t>(u & 0xffu));
            compressedIdentity.push_back(static_cast<std::uint8_t>((u >> 8u) & 0xffu));
        }
    }

    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        8u * multiplicity,
        sequences,
        externalBySequence);
    auto flattened = FlattenLegacyValueTrack(
        sourceTrack,
        windows,
        compressedIdentity,
        LegacySingleKeyPolicy::ConstantNoRanges);
    for (auto& key : flattened.keys)
        key = ExpandWotlkQuaternionKey(key, interpolation);
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

std::array<std::uint8_t, 28> ConvertEnabledTrack(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto interpolation = static_cast<std::int16_t>(ReadU16(source, trackOffset));
    const std::size_t multiplicity = (interpolation == 2 || interpolation == 3) ? 3u : 1u;
    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        multiplicity,
        sequences,
        externalBySequence);

    FlattenedLegacyTrack flattened;
    if (sourceTrack.timestamps.empty())
    {
        // Real V4.4 attachment Golden: an empty WotLK enabled outer array is
        // serialized by successful legacy targets as a constant enabled track.
        // Do not emit an empty Classic track here.
        flattened.timestamps.push_back(0u);
        flattened.keys.push_back(std::vector<std::uint8_t>(multiplicity, 1u));
    }
    else
    {
        flattened = FlattenLegacyValueTrack(
            sourceTrack,
            windows,
            std::vector<std::uint8_t>(multiplicity, 1u),
            LegacySingleKeyPolicy::ConstantNoRanges);
    }
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

M2ArrayRef ReserveTarget(
    BinaryBuilder& output,
    const std::uint32_t count,
    const std::size_t stride)
{
    M2ArrayRef result{count, 0u};
    if (count != 0u)
        result.offset = output.Reserve(static_cast<std::size_t>(count) * stride);
    return result;
}

} // namespace

M2ArrayRef ConvertWotlkColors(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceColors,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    constexpr std::size_t sourceStride = 40u;
    constexpr std::size_t targetStride = 56u;
    ValidateRecords(source, sourceColors, sourceStride, "Color");
    const auto target = ReserveTarget(output, sourceColors.count, targetStride);
    const std::vector<std::uint8_t> zeroVec3(12u, 0u);
    const std::vector<std::uint8_t> opaqueShort{0xffu, 0x7fu};

    for (std::uint32_t i = 0u; i < sourceColors.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceColors.offset) + static_cast<std::size_t>(i) * sourceStride;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(targetStride);
        std::array<std::uint8_t, targetStride> rec{};
        const auto rgb = ConvertRawValueTrack(output, source, so + 0u, 12u, zeroVec3, windows, sequences, externalBySequence);
        const auto alpha = ConvertRawValueTrack(output, source, so + 20u, 2u, opaqueShort, windows, sequences, externalBySequence);
        std::copy(rgb.begin(), rgb.end(), rec.begin() + 0u);
        std::copy(alpha.begin(), alpha.end(), rec.begin() + 28u);
        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

M2ArrayRef ConvertWotlkTransparency(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceTransparency,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    constexpr std::size_t sourceStride = 20u;
    constexpr std::size_t targetStride = 28u;
    ValidateRecords(source, sourceTransparency, sourceStride, "Transparency");
    const auto target = ReserveTarget(output, sourceTransparency.count, targetStride);
    const std::vector<std::uint8_t> opaqueShort{0xffu, 0x7fu};

    for (std::uint32_t i = 0u; i < sourceTransparency.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceTransparency.offset) + static_cast<std::size_t>(i) * sourceStride;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(targetStride);
        const auto alpha = ConvertRawValueTrack(output, source, so, 2u, opaqueShort, windows, sequences, externalBySequence);
        output.Patch(to, alpha.data(), alpha.size());
    }
    return target;
}

M2ArrayRef ConvertWotlkTextureAnimations(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceTextureAnimations,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    constexpr std::size_t sourceStride = 60u;
    constexpr std::size_t targetStride = 84u;
    ValidateRecords(source, sourceTextureAnimations, sourceStride, "TextureAnimation");
    const auto target = ReserveTarget(output, sourceTextureAnimations.count, targetStride);
    const std::vector<std::uint8_t> zeroVec3(12u, 0u);

    for (std::uint32_t i = 0u; i < sourceTextureAnimations.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceTextureAnimations.offset) + static_cast<std::size_t>(i) * sourceStride;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(targetStride);
        std::array<std::uint8_t, targetStride> rec{};
        const auto translation = ConvertRawValueTrack(output, source, so + 0u, 12u, zeroVec3, windows, sequences, externalBySequence);
        const auto rotation = ConvertQuaternionTrack(output, source, so + 20u, windows, sequences, externalBySequence);
        const auto scaling = ConvertRawValueTrack(output, source, so + 40u, 12u, zeroVec3, windows, sequences, externalBySequence);
        std::copy(translation.begin(), translation.end(), rec.begin() + 0u);
        std::copy(rotation.begin(), rotation.end(), rec.begin() + 28u);
        std::copy(scaling.begin(), scaling.end(), rec.begin() + 56u);
        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

M2ArrayRef ConvertWotlkAttachments(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceAttachments,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    constexpr std::size_t sourceStride = 40u;
    constexpr std::size_t targetStride = 48u;
    ValidateRecords(source, sourceAttachments, sourceStride, "Attachment");
    const auto target = ReserveTarget(output, sourceAttachments.count, targetStride);

    for (std::uint32_t i = 0u; i < sourceAttachments.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceAttachments.offset) + static_cast<std::size_t>(i) * sourceStride;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(targetStride);
        std::array<std::uint8_t, targetStride> rec{};
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(so), 20u, rec.begin());
        const auto enabled = ConvertEnabledTrack(output, source, so + 20u, windows, sequences, externalBySequence);
        std::copy(enabled.begin(), enabled.end(), rec.begin() + 20u);
        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

} // namespace turtle335::m2
