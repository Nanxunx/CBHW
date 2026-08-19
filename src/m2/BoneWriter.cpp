#include "turtle335/m2/BoneWriter.h"

#include "turtle335/m2/ExternalAnim.h"
#include "turtle335/m2/LegacyTrack.h"
#include "turtle335/m2/QuaternionCodec.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kSourceBoneSize = 88u;
constexpr std::size_t kTargetBoneSize = 108u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error("Bone track interpolation OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

std::vector<std::uint8_t> Float3Key(
    const float x,
    const float y,
    const float z,
    const std::size_t multiplicity)
{
    std::vector<std::uint8_t> out;
    out.reserve(12u * multiplicity);
    const std::array<float, 3> values{{x, y, z}};
    for (std::size_t n = 0u; n < multiplicity; ++n)
    {
        for (const float value : values)
        {
            std::uint32_t bits = 0u;
            std::memcpy(&bits, &value, sizeof(bits));
            out.push_back(static_cast<std::uint8_t>(bits & 0xffu));
            out.push_back(static_cast<std::uint8_t>((bits >> 8u) & 0xffu));
            out.push_back(static_cast<std::uint8_t>((bits >> 16u) & 0xffu));
            out.push_back(static_cast<std::uint8_t>((bits >> 24u) & 0xffu));
        }
    }
    return out;
}

std::vector<std::uint8_t> CompressedIdentityQuaternion(const std::size_t multiplicity)
{
    std::vector<std::uint8_t> out;
    out.reserve(8u * multiplicity);
    constexpr std::array<std::int16_t, 4> identity{{-32767, -32767, -32767, -1}};
    for (std::size_t n = 0u; n < multiplicity; ++n)
    {
        for (const auto value : identity)
        {
            const auto u = static_cast<std::uint16_t>(value);
            out.push_back(static_cast<std::uint8_t>(u & 0xffu));
            out.push_back(static_cast<std::uint8_t>((u >> 8u) & 0xffu));
        }
    }
    return out;
}

std::array<std::uint8_t, 28> ConvertValueTrack(
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
    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        8u * multiplicity,
        sequences,
        externalBySequence);
    auto flattened = FlattenLegacyValueTrack(
        sourceTrack,
        windows,
        CompressedIdentityQuaternion(multiplicity),
        LegacySingleKeyPolicy::ConstantNoRanges);
    for (auto& key : flattened.keys)
        key = ExpandWotlkQuaternionKey(key, interpolation);
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

} // namespace

BoneConversionResult ConvertWotlkBones(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceBones,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    BoneConversionResult result;
    result.target.count = sourceBones.count;
    if (sourceBones.count == 0u)
        return result;

    const std::size_t sourceBytes = static_cast<std::size_t>(sourceBones.count) * kSourceBoneSize;
    if (sourceBones.offset == 0u || static_cast<std::size_t>(sourceBones.offset) > source.size() ||
        sourceBytes > source.size() - static_cast<std::size_t>(sourceBones.offset))
        throw std::runtime_error("WotLK bone records OOB");

    result.target.offset = output.Reserve(static_cast<std::size_t>(sourceBones.count) * kTargetBoneSize);

    const std::vector<std::uint8_t> zeroVec3(12u, 0u);
    const auto identityScale = Float3Key(1.0f, 1.0f, 1.0f, 1u);

    for (std::uint32_t i = 0u; i < sourceBones.count; ++i)
    {
        const std::size_t sourceRecord = static_cast<std::size_t>(sourceBones.offset) +
                                         static_cast<std::size_t>(i) * kSourceBoneSize;
        const std::uint32_t targetRecord = result.target.offset + i * static_cast<std::uint32_t>(kTargetBoneSize);
        std::array<std::uint8_t, kTargetBoneSize> record{};

        // animid / flags / parent / geoset. WotLK's extra int32 at +12 is
        // intentionally absent from the canonical Classic/Turtle bone.
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(sourceRecord), 12u, record.begin());

        const auto translation = ConvertValueTrack(
            output, source, sourceRecord + 16u, 12u, zeroVec3,
            windows, sequences, externalBySequence);
        std::copy(translation.begin(), translation.end(), record.begin() + 12u);

        const auto rotation = ConvertQuaternionTrack(
            output, source, sourceRecord + 36u,
            windows, sequences, externalBySequence);
        std::copy(rotation.begin(), rotation.end(), record.begin() + 40u);

        const auto scaling = ConvertValueTrack(
            output, source, sourceRecord + 56u, 12u, identityScale,
            windows, sequences, externalBySequence);
        std::copy(scaling.begin(), scaling.end(), record.begin() + 68u);

        std::copy_n(
            source.begin() + static_cast<std::ptrdiff_t>(sourceRecord + 76u),
            12u,
            record.begin() + 96u);

        output.Patch(targetRecord, record.data(), record.size());
    }

    return result;
}

} // namespace turtle335::m2
