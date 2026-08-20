#include "turtle335/m2/CameraWriter.h"

#include "turtle335/m2/ExternalAnim.h"
#include "turtle335/m2/LegacyTrack.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kSourceCameraSize = 100u;
constexpr std::size_t kTargetCameraSize = 124u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error("Camera interpolation OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

void ValidateRecords(const std::vector<std::uint8_t>& source, const M2ArrayRef ref)
{
    if (ref.count == 0u)
        return;
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * kSourceCameraSize;
    if (ref.offset == 0u || static_cast<std::size_t>(ref.offset) > source.size() ||
        bytes > source.size() - static_cast<std::size_t>(ref.offset))
        throw std::runtime_error("WotLK Camera records OOB");
}

std::array<std::uint8_t, 28> ConvertFixedKeyTrack(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::size_t physicalKeySize,
    const std::vector<std::uint8_t>& defaultKey,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto sourceTrack = ParseWotlkTrackWithExternal(
        source,
        trackOffset,
        physicalKeySize,
        sequences,
        externalBySequence);
    const auto flattened = FlattenLegacyValueTrack(
        sourceTrack,
        windows,
        defaultKey,
        LegacySingleKeyPolicy::ConstantNoRanges);
    return SerializeClassicTrack(output, sourceTrack, flattened);
}

std::array<std::uint8_t, 28> ConvertVec3Track(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t trackOffset,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    const auto interpolation = static_cast<std::int16_t>(ReadU16(source, trackOffset));
    const std::size_t multiplicity = (interpolation == 2 || interpolation == 3) ? 3u : 1u;
    return ConvertFixedKeyTrack(
        output,
        source,
        trackOffset,
        12u * multiplicity,
        std::vector<std::uint8_t>(12u * multiplicity, 0u),
        windows,
        sequences,
        externalBySequence);
}

} // namespace

M2ArrayRef ConvertWotlkCameras(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceCameras,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    ValidateRecords(source, sourceCameras);
    M2ArrayRef target{sourceCameras.count, 0u};
    if (sourceCameras.count == 0u)
        return target;

    target.offset = output.Reserve(static_cast<std::size_t>(sourceCameras.count) * kTargetCameraSize);

    for (std::uint32_t i = 0u; i < sourceCameras.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceCameras.offset) +
                               static_cast<std::size_t>(i) * kSourceCameraSize;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(kTargetCameraSize);
        std::array<std::uint8_t, kTargetCameraSize> rec{};

        // Type/FOV/far/near are byte-identical.
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(so), 16u, rec.begin());

        // WotLK Camera translation keys use the historical BigFloat physical
        // payload: Vec3[3] = 36 bytes, independent of interpolation type. This
        // was confirmed against 25/25 V4.4 selected Golden camera records.
        const auto transPosition = ConvertFixedKeyTrack(
            output,
            source,
            so + 16u,
            36u,
            std::vector<std::uint8_t>(36u, 0u),
            windows,
            sequences,
            externalBySequence);
        std::copy(transPosition.begin(), transPosition.end(), rec.begin() + 16u);

        // Static camera position shifts after the expanded track.
        std::copy_n(
            source.begin() + static_cast<std::ptrdiff_t>(so + 36u),
            12u,
            rec.begin() + 44u);

        const auto transTarget = ConvertFixedKeyTrack(
            output,
            source,
            so + 48u,
            36u,
            std::vector<std::uint8_t>(36u, 0u),
            windows,
            sequences,
            externalBySequence);
        std::copy(transTarget.begin(), transTarget.end(), rec.begin() + 56u);

        std::copy_n(
            source.begin() + static_cast<std::ptrdiff_t>(so + 68u),
            12u,
            rec.begin() + 84u);

        // Historical headers call this block scaling; modern descriptions call
        // it roll. The binary rule is the same: Vec3 WotLK track -> 28B legacy.
        const auto roll = ConvertVec3Track(
            output,
            source,
            so + 80u,
            windows,
            sequences,
            externalBySequence);
        std::copy(roll.begin(), roll.end(), rec.begin() + 96u);

        output.Patch(to, rec.data(), rec.size());
    }

    return target;
}

} // namespace turtle335::m2
