#include "turtle335/m2/ExternalAnim.h"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace turtle335::m2 {
namespace {

std::uint16_t ReadU16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const char* what)
{
    if (offset > bytes.size() || 2u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " outside bytes");
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u);
}

std::int16_t ReadI16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const char* what)
{
    return static_cast<std::int16_t>(ReadU16(bytes, offset, what));
}

std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " outside bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

std::pair<std::uint32_t, std::uint32_t> ReadPair(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const char* what)
{
    return {
        ReadU32(bytes, offset, what),
        ReadU32(bytes, offset + 4u, what),
    };
}

void ValidatePayload(
    const std::vector<std::uint8_t>& bytes,
    const std::uint32_t count,
    const std::uint32_t offset,
    const std::size_t elementSize,
    const char* what,
    const bool allowZeroOffset)
{
    if (count == 0u)
        return;
    if (elementSize == 0u)
        throw std::runtime_error(std::string(what) + " has zero element size");
    const std::size_t size = static_cast<std::size_t>(count) * elementSize;
    if ((!allowZeroOffset && offset == 0u) ||
        static_cast<std::size_t>(offset) > bytes.size() ||
        size > bytes.size() - static_cast<std::size_t>(offset))
        throw std::runtime_error(std::string(what) + " payload outside selected data source");
}

const std::vector<std::uint8_t>& SelectPayloadSource(
    const std::vector<std::uint8_t>& mainM2,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence,
    const std::size_t outerCount,
    const std::size_t groupIndex,
    const std::uint32_t innerCount)
{
    // Global/free-standing tracks normally have one outer group even when the
    // model has many sequences. They are therefore owned by the main M2.
    if (outerCount != sequences.size() || groupIndex >= sequences.size() || innerCount == 0u)
        return mainM2;

    const std::size_t payloadIndex = ResolveWotlkPayloadSequenceIndex(sequences, groupIndex);
    if (!SequenceUsesExternalAnimSidecar(sequences[payloadIndex]))
        return mainM2;

    if (payloadIndex >= externalBySequence.size() || externalBySequence[payloadIndex] == nullptr)
        throw std::runtime_error(
            "WotLK track requires an external .anim sidecar that was not supplied");
    return *externalBySequence[payloadIndex];
}

} // namespace

bool SequenceUsesExternalAnimSidecar(const WotlkM2Sequence& sequence) noexcept
{
    return (sequence.flags & 0x20u) == 0u;
}

std::size_t ResolveWotlkPayloadSequenceIndex(
    const std::vector<WotlkM2Sequence>& sequences,
    const std::size_t sequenceIndex)
{
    if (sequenceIndex >= sequences.size())
        throw std::runtime_error("WotLK animation payload sequence index is out of range");

    std::vector<bool> visited(sequences.size(), false);
    std::size_t current = sequenceIndex;
    while ((sequences[current].flags & 0x40u) != 0u)
    {
        if (visited[current])
            throw std::runtime_error("WotLK animation alias sequence contains a cycle");
        visited[current] = true;

        const std::size_t next = static_cast<std::size_t>(sequences[current].index);
        if (next >= sequences.size())
            throw std::runtime_error("WotLK animation alias target is out of range");
        current = next;
    }
    return current;
}

std::string BuildWotlkAnimSidecarFilename(
    const std::string_view modelStem,
    const WotlkM2Sequence& sequence)
{
    std::ostringstream out;
    out << modelStem
        << std::setfill('0') << std::setw(4) << sequence.animationId
        << '-'
        << std::setfill('0') << std::setw(2) << sequence.subAnimationId
        << ".anim";
    return out.str();
}

WotlkTrackData ParseWotlkTrackWithExternal(
    const std::vector<std::uint8_t>& mainM2,
    const std::size_t trackOffset,
    const std::size_t keySize,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    if (keySize == 0u)
        throw std::runtime_error("WotLK track key size cannot be zero");
    if (trackOffset > mainM2.size() || 20u > mainM2.size() - trackOffset)
        throw std::runtime_error("WotLK track header outside main M2");

    WotlkTrackData result;
    result.interpolationType = ReadI16(mainM2, trackOffset + 0u, "track interpolation");
    result.globalSequence = ReadI16(mainM2, trackOffset + 2u, "track global sequence");

    const auto timeOuter = ReadPair(mainM2, trackOffset + 4u, "track times outer");
    const auto keyOuter = ReadPair(mainM2, trackOffset + 12u, "track keys outer");
    if (timeOuter.first != keyOuter.first)
        throw std::runtime_error("WotLK track outer time/key count mismatch");

    const std::size_t outerCount = static_cast<std::size_t>(timeOuter.first);
    if (outerCount != 0u)
    {
        const std::size_t refsBytes = outerCount * 8u;
        if (timeOuter.second == 0u || static_cast<std::size_t>(timeOuter.second) > mainM2.size() ||
            refsBytes > mainM2.size() - static_cast<std::size_t>(timeOuter.second))
            throw std::runtime_error("WotLK time-group refs outside main M2");
        if (keyOuter.second == 0u || static_cast<std::size_t>(keyOuter.second) > mainM2.size() ||
            refsBytes > mainM2.size() - static_cast<std::size_t>(keyOuter.second))
            throw std::runtime_error("WotLK key-group refs outside main M2");
    }

    result.timestamps.resize(outerCount);
    result.keys.resize(outerCount);

    for (std::size_t i = 0; i < outerCount; ++i)
    {
        const auto times = ReadPair(
            mainM2,
            static_cast<std::size_t>(timeOuter.second) + i * 8u,
            "track time group");
        const auto keys = ReadPair(
            mainM2,
            static_cast<std::size_t>(keyOuter.second) + i * 8u,
            "track key group");
        if (times.first != keys.first)
            throw std::runtime_error("WotLK track inner time/key count mismatch");

        const auto& timeSource = SelectPayloadSource(
            mainM2, sequences, externalBySequence, outerCount, i, times.first);
        const auto& keySource = SelectPayloadSource(
            mainM2, sequences, externalBySequence, outerCount, i, keys.first);

        ValidatePayload(
            timeSource,
            times.first,
            times.second,
            4u,
            "WotLK timestamps",
            &timeSource != &mainM2);
        ValidatePayload(
            keySource,
            keys.first,
            keys.second,
            keySize,
            "WotLK keys",
            &keySource != &mainM2);

        result.timestamps[i].reserve(times.first);
        for (std::uint32_t j = 0u; j < times.first; ++j)
        {
            result.timestamps[i].push_back(ReadU32(
                timeSource,
                static_cast<std::size_t>(times.second) + static_cast<std::size_t>(j) * 4u,
                "track timestamp"));
        }

        result.keys[i].reserve(keys.first);
        for (std::uint32_t j = 0u; j < keys.first; ++j)
        {
            const std::size_t at = static_cast<std::size_t>(keys.second) +
                                   static_cast<std::size_t>(j) * keySize;
            result.keys[i].emplace_back(
                keySource.begin() + static_cast<std::ptrdiff_t>(at),
                keySource.begin() + static_cast<std::ptrdiff_t>(at + keySize));
        }
    }

    return result;
}

} // namespace turtle335::m2
