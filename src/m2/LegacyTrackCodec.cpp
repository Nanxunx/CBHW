#include "turtle335/m2/LegacyTrackCodec.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace turtle335::m2 {
namespace {

void CheckSpan(const std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::size_t size, const char* what)
{
    if (offset > bytes.size() || size > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside bytes");
}

std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* what)
{
    CheckSpan(bytes, offset, 2u, what);
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8u);
}

std::int16_t ReadI16(const std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* what)
{
    return static_cast<std::int16_t>(ReadU16(bytes, offset, what));
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* what)
{
    CheckSpan(bytes, offset, 4u, what);
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

void WriteU16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value)
{
    CheckSpan(bytes, offset, 2u, "target uint16");
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
}

void WriteI16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::int16_t value)
{
    WriteU16(bytes, offset, static_cast<std::uint16_t>(value));
}

void WriteU32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value)
{
    CheckSpan(bytes, offset, 4u, "target uint32");
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
}

std::pair<std::uint32_t, std::uint32_t> ReadPair(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const char* what)
{
    return {ReadU32(bytes, offset, what), ReadU32(bytes, offset + 4u, what)};
}

std::vector<std::vector<std::vector<std::uint8_t>>> ReadNestedRaw(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t pairOffset,
    const std::size_t elementSize,
    const char* what)
{
    const auto outer = ReadPair(bytes, pairOffset, what);
    std::vector<std::vector<std::vector<std::uint8_t>>> result;
    result.reserve(outer.first);
    if (outer.first == 0u)
        return result;

    CheckSpan(bytes, outer.second, static_cast<std::size_t>(outer.first) * 8u, what);
    for (std::uint32_t i = 0; i < outer.first; ++i)
    {
        const auto inner = ReadPair(bytes, static_cast<std::size_t>(outer.second) + static_cast<std::size_t>(i) * 8u, what);
        CheckSpan(bytes, inner.second, static_cast<std::size_t>(inner.first) * elementSize, what);
        std::vector<std::vector<std::uint8_t>> group;
        group.reserve(inner.first);
        for (std::uint32_t j = 0; j < inner.first; ++j)
        {
            const std::size_t beginOffset = static_cast<std::size_t>(inner.second) + static_cast<std::size_t>(j) * elementSize;
            const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(beginOffset);
            group.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(elementSize));
        }
        result.push_back(std::move(group));
    }
    return result;
}

std::vector<std::vector<std::uint32_t>> ReadNestedTimes(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t pairOffset)
{
    const auto raw = ReadNestedRaw(bytes, pairOffset, 4u, "track timestamps");
    std::vector<std::vector<std::uint32_t>> result;
    result.reserve(raw.size());
    for (const auto& group : raw)
    {
        std::vector<std::uint32_t> times;
        times.reserve(group.size());
        for (const auto& item : group)
        {
            const std::uint32_t value =
                static_cast<std::uint32_t>(item[0]) |
                (static_cast<std::uint32_t>(item[1]) << 8u) |
                (static_cast<std::uint32_t>(item[2]) << 16u) |
                (static_cast<std::uint32_t>(item[3]) << 24u);
            times.push_back(value);
        }
        result.push_back(std::move(times));
    }
    return result;
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> ClassicRanges(
    const std::vector<std::size_t>& counts)
{
    std::uint64_t cursor = 0u;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    out.reserve(counts.size() + 1u);
    for (const auto count : counts)
    {
        const auto start = cursor;
        if (count == 1u)
            cursor += 1u;
        else if (count > 1u)
            cursor += count - 1u;
        else
            cursor += 1u;

        if (cursor > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("Classic track range cursor exceeds uint32");
        out.emplace_back(static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(cursor));
        ++cursor;
    }
    out.emplace_back(0u, 0u);
    return out;
}

std::vector<std::uint8_t> FlattenRanges(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& ranges)
{
    std::vector<std::uint8_t> out(ranges.size() * 8u, 0u);
    for (std::size_t i = 0; i < ranges.size(); ++i)
    {
        WriteU32(out, i * 8u, ranges[i].first);
        WriteU32(out, i * 8u + 4u, ranges[i].second);
    }
    return out;
}

std::vector<std::uint8_t> FlattenTimes(const std::vector<std::uint32_t>& times)
{
    std::vector<std::uint8_t> out(times.size() * 4u, 0u);
    for (std::size_t i = 0; i < times.size(); ++i)
        WriteU32(out, i * 4u, times[i]);
    return out;
}

std::vector<std::uint8_t> FlattenValues(const std::vector<std::vector<std::uint8_t>>& values)
{
    std::size_t size = 0u;
    for (const auto& value : values)
        size += value.size();

    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (const auto& value : values)
        out.insert(out.end(), value.begin(), value.end());
    return out;
}

} // namespace

WotlkTrackData ParseWotlkTrack(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::size_t baseKeySize)
{
    CheckSpan(bytes, offset, 20u, "WotLK track");
    WotlkTrackData result;
    result.interpolation = ReadU16(bytes, offset, "track interpolation");
    result.globalSequence = ReadI16(bytes, offset + 2u, "track global sequence");

    if (baseKeySize == 0u)
        throw std::runtime_error("track base key size is zero");
    result.keySize = baseKeySize * ((result.interpolation == 2u || result.interpolation == 3u) ? 3u : 1u);

    result.times = ReadNestedTimes(bytes, offset + 4u);
    result.values = ReadNestedRaw(bytes, offset + 12u, result.keySize, "track keys");
    if (result.times.size() != result.values.size())
        throw std::runtime_error("track outer timestamp/key count mismatch");
    return result;
}

ClassicTrackData ConvertLegacyTrack(
    const WotlkTrackData& source,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<std::uint8_t>& baseDefault,
    const EmptyOuterMode emptyMode)
{
    const std::size_t multiplier =
        (source.interpolation == 2u || source.interpolation == 3u) ? 3u : 1u;
    if (baseDefault.size() * multiplier != source.keySize)
        throw std::runtime_error("track semantic default size does not match key size");

    ClassicTrackData out;
    out.interpolation = source.interpolation;
    out.globalSequence = source.globalSequence;
    out.keySize = source.keySize;

    std::vector<std::uint8_t> defaultValue;
    defaultValue.reserve(source.keySize);
    for (std::size_t i = 0; i < multiplier; ++i)
        defaultValue.insert(defaultValue.end(), baseDefault.begin(), baseDefault.end());

    if (source.times.empty())
    {
        if (emptyMode == EmptyOuterMode::EnabledOne)
        {
            if (source.keySize != 1u)
                throw std::runtime_error("EnabledOne requires 1-byte keys");
            out.times.push_back(0u);
            out.values.push_back({1u});
        }
        return out;
    }

    if (source.globalSequence >= 0)
    {
        out.times = source.times.front();
        out.values = source.values.front();
        return out;
    }

    if (source.times.size() == windows.size())
    {
        std::vector<std::size_t> counts;
        counts.reserve(windows.size());
        for (std::size_t i = 0; i < windows.size(); ++i)
        {
            const auto& times = source.times[i];
            const auto& values = source.values[i];
            if (times.size() != values.size())
                throw std::runtime_error("track inner timestamp/key count mismatch");

            counts.push_back(times.size());
            if (times.empty())
            {
                out.times.push_back(windows[i].start);
                out.times.push_back(windows[i].end);
                out.values.push_back(defaultValue);
                out.values.push_back(defaultValue);
            }
            else if (times.size() == 1u)
            {
                out.times.push_back(windows[i].start + times.front());
                out.times.push_back(windows[i].end + times.front());
                out.values.push_back(values.front());
                out.values.push_back(values.front());
            }
            else
            {
                for (std::size_t j = 0; j < times.size(); ++j)
                {
                    out.times.push_back(windows[i].start + times[j]);
                    out.values.push_back(values[j]);
                }
            }
        }
        out.ranges = ClassicRanges(counts);
        return out;
    }

    if (source.times.size() == 1u)
    {
        if (source.times.front().size() != source.values.front().size())
            throw std::runtime_error("free-standing track timestamp/key count mismatch");
        out.times = source.times.front();
        out.values = source.values.front();
        return out;
    }

    throw std::runtime_error("legacy track outer count is neither sequence count nor one");
}

AbsoluteBlockBuilder::AbsoluteBlockBuilder(
    const std::size_t fixedSize,
    const std::uint32_t absoluteOffset)
    : fixed_(fixedSize, 0u), absoluteOffset_(absoluteOffset)
{
}

std::vector<std::uint8_t>& AbsoluteBlockBuilder::Fixed()
{
    return fixed_;
}

const std::vector<std::uint8_t>& AbsoluteBlockBuilder::Fixed() const
{
    return fixed_;
}

std::uint32_t AbsoluteBlockBuilder::Append(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t alignment)
{
    if (bytes.empty())
        return 0u;
    if (alignment == 0u)
        throw std::runtime_error("block alignment cannot be zero");
    while (payload_.size() % alignment != 0u)
        payload_.push_back(0u);

    const std::uint64_t absolute =
        static_cast<std::uint64_t>(absoluteOffset_) +
        static_cast<std::uint64_t>(fixed_.size()) +
        static_cast<std::uint64_t>(payload_.size());
    if (absolute > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("block payload absolute offset exceeds uint32");

    payload_.insert(payload_.end(), bytes.begin(), bytes.end());
    return static_cast<std::uint32_t>(absolute);
}

std::vector<std::uint8_t> AbsoluteBlockBuilder::Finish() const
{
    std::vector<std::uint8_t> result;
    result.reserve(fixed_.size() + payload_.size());
    result.insert(result.end(), fixed_.begin(), fixed_.end());
    result.insert(result.end(), payload_.begin(), payload_.end());
    return result;
}

void SerializeClassicTrack(
    AbsoluteBlockBuilder& builder,
    const std::size_t fixedOffset,
    const ClassicTrackData& track)
{
    auto& fixed = builder.Fixed();
    if (fixedOffset > fixed.size() || 28u > fixed.size() - fixedOffset)
        throw std::runtime_error("Classic track fixed record is outside target block");

    const auto ranges = FlattenRanges(track.ranges);
    const auto times = FlattenTimes(track.times);
    const auto values = FlattenValues(track.values);
    const auto rangeOffset = builder.Append(ranges, 4u);
    const auto timeOffset = builder.Append(times, 4u);
    const auto valueOffset = builder.Append(values, 4u);

    WriteU16(fixed, fixedOffset, track.interpolation);
    WriteI16(fixed, fixedOffset + 2u, track.globalSequence);
    WriteU32(fixed, fixedOffset + 4u, static_cast<std::uint32_t>(track.ranges.size()));
    WriteU32(fixed, fixedOffset + 8u, rangeOffset);
    WriteU32(fixed, fixedOffset + 12u, static_cast<std::uint32_t>(track.times.size()));
    WriteU32(fixed, fixedOffset + 16u, timeOffset);
    WriteU32(fixed, fixedOffset + 20u, static_cast<std::uint32_t>(track.values.size()));
    WriteU32(fixed, fixedOffset + 24u, valueOffset);
}

} // namespace turtle335::m2
