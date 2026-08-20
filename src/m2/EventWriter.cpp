#include "turtle335/m2/EventWriter.h"

#include "turtle335/m2/ExternalAnim.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kSourceEventSize = 36u;
constexpr std::size_t kTargetEventSize = 44u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error(std::string(what) + " OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

std::int16_t ReadI16(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    return static_cast<std::int16_t>(ReadU16(d, o, what));
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    if (o > d.size() || 4u > d.size() - o)
        throw std::runtime_error(std::string(what) + " OOB");
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
           (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
           (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
}

std::pair<std::uint32_t, std::uint32_t> ReadPair(
    const std::vector<std::uint8_t>& d,
    const std::size_t o,
    const char* what)
{
    return {ReadU32(d, o, what), ReadU32(d, o + 4u, what)};
}

void PutU16(std::uint8_t* d, const std::size_t o, const std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
}

void PutU32(std::uint8_t* d, const std::size_t o, const std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
    d[o + 2u] = static_cast<std::uint8_t>((v >> 16u) & 0xffu);
    d[o + 3u] = static_cast<std::uint8_t>((v >> 24u) & 0xffu);
}

void ValidateRecords(const std::vector<std::uint8_t>& source, const M2ArrayRef ref)
{
    if (ref.count == 0u)
        return;
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * kSourceEventSize;
    if (ref.offset == 0u || static_cast<std::size_t>(ref.offset) > source.size() ||
        bytes > source.size() - static_cast<std::size_t>(ref.offset))
        throw std::runtime_error("WotLK Event records OOB");
}

const std::vector<std::uint8_t>& SelectTimeSource(
    const std::vector<std::uint8_t>& mainM2,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& sidecars,
    const std::size_t outerCount,
    const std::size_t group,
    const std::uint32_t count)
{
    if (count == 0u || outerCount != sequences.size() || group >= sequences.size())
        return mainM2;

    const std::size_t payloadIndex = ResolveWotlkPayloadSequenceIndex(sequences, group);
    if (!SequenceUsesExternalAnimSidecar(sequences[payloadIndex]))
        return mainM2;
    if (payloadIndex >= sidecars.size() || sidecars[payloadIndex] == nullptr)
        throw std::runtime_error("Event timer requires missing external .anim sidecar");
    return *sidecars[payloadIndex];
}

std::uint32_t AppendRanges(
    BinaryBuilder& output,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& ranges)
{
    if (ranges.empty())
        return 0u;
    std::vector<std::uint8_t> raw(ranges.size() * 8u, 0u);
    for (std::size_t i = 0u; i < ranges.size(); ++i)
    {
        PutU32(raw.data(), i * 8u, ranges[i].first);
        PutU32(raw.data(), i * 8u + 4u, ranges[i].second);
    }
    return output.Append(raw);
}

std::uint32_t AppendTimes(BinaryBuilder& output, const std::vector<std::uint32_t>& times)
{
    if (times.empty())
        return 0u;
    std::vector<std::uint8_t> raw(times.size() * 4u, 0u);
    for (std::size_t i = 0u; i < times.size(); ++i)
        PutU32(raw.data(), i * 4u, times[i]);
    return output.Append(raw);
}

std::array<std::uint8_t, 20> ConvertEventTimer(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const std::size_t timerOffset,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    if (timerOffset > source.size() || 12u > source.size() - timerOffset)
        throw std::runtime_error("WotLK Event timer OOB");

    const auto interpolation = ReadI16(source, timerOffset + 0u, "Event interpolation");
    const auto globalSequence = ReadI16(source, timerOffset + 2u, "Event global sequence");
    const auto outer = ReadPair(source, timerOffset + 4u, "Event times outer");
    const std::size_t outerCount = static_cast<std::size_t>(outer.first);
    if (outerCount != 0u)
    {
        const std::size_t refsBytes = outerCount * 8u;
        if (outer.second == 0u || static_cast<std::size_t>(outer.second) > source.size() ||
            refsBytes > source.size() - static_cast<std::size_t>(outer.second))
            throw std::runtime_error("Event time group refs OOB");
    }

    std::vector<std::vector<std::uint32_t>> groups(outerCount);
    for (std::size_t i = 0u; i < outerCount; ++i)
    {
        const auto ref = ReadPair(
            source,
            static_cast<std::size_t>(outer.second) + i * 8u,
            "Event time group");
        const auto& payload = SelectTimeSource(
            source, sequences, externalBySequence, outerCount, i, ref.first);
        if (ref.first != 0u)
        {
            const std::size_t bytes = static_cast<std::size_t>(ref.first) * 4u;
            const bool externalPayload = &payload != &source;
            if ((!externalPayload && ref.second == 0u) ||
                static_cast<std::size_t>(ref.second) > payload.size() ||
                bytes > payload.size() - static_cast<std::size_t>(ref.second))
                throw std::runtime_error("Event time payload OOB");
            groups[i].reserve(ref.first);
            for (std::uint32_t j = 0u; j < ref.first; ++j)
                groups[i].push_back(ReadU32(
                    payload,
                    static_cast<std::size_t>(ref.second) + static_cast<std::size_t>(j) * 4u,
                    "Event timestamp"));
        }
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    std::vector<std::uint32_t> times;

    if (groups.empty())
    {
        // Truly empty timers stay empty.
    }
    else if (globalSequence >= 0)
    {
        if (groups.size() != 1u)
            throw std::runtime_error("global Event timer must have one time group");
        times = groups.front();
    }
    else if (groups.size() == 1u)
    {
        // Paired successful 1.12 targets preserve a free-standing one-group
        // timer's relative times and retain one explicit range. Do not shift
        // it into a sequence window merely because the model has one sequence.
        if (!groups[0].empty())
        {
            ranges.emplace_back(0u, static_cast<std::uint32_t>(groups[0].size()));
            times = groups[0];
        }
    }
    else if (groups.size() == windows.size())
    {
        // V4.6 selected paired Golden rule, verified across every Event record
        // in the selected corpus:
        //   empty group  -> empty range
        //   one time     -> duplicate at sequence start/end
        //   multiple     -> shift all by sequence start
        // then append the historical [0,0] sentinel range.
        std::uint32_t cursor = 0u;
        ranges.reserve(windows.size() + 1u);
        for (std::size_t i = 0u; i < windows.size(); ++i)
        {
            const auto& group = groups[i];
            if (group.empty())
            {
                ranges.emplace_back(cursor, cursor);
            }
            else if (group.size() == 1u)
            {
                ranges.emplace_back(cursor, cursor + 2u);
                times.push_back(windows[i].start + group[0]);
                times.push_back(windows[i].end + group[0]);
                cursor += 2u;
            }
            else
            {
                const auto count = static_cast<std::uint32_t>(group.size());
                ranges.emplace_back(cursor, cursor + count);
                for (const auto relative : group)
                    times.push_back(windows[i].start + relative);
                cursor += count;
            }
        }
        ranges.emplace_back(0u, 0u);
    }
    else
    {
        throw std::runtime_error("Event timer outer count is neither sequence count nor one");
    }

    std::array<std::uint8_t, 20> rec{};
    PutU16(rec.data(), 0u, static_cast<std::uint16_t>(interpolation));
    PutU16(rec.data(), 2u, static_cast<std::uint16_t>(globalSequence));
    PutU32(rec.data(), 4u, static_cast<std::uint32_t>(ranges.size()));
    PutU32(rec.data(), 8u, AppendRanges(output, ranges));
    PutU32(rec.data(), 12u, static_cast<std::uint32_t>(times.size()));
    PutU32(rec.data(), 16u, AppendTimes(output, times));
    return rec;
}

} // namespace

M2ArrayRef ConvertWotlkEvents(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceEvents,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    ValidateRecords(source, sourceEvents);
    M2ArrayRef target{sourceEvents.count, 0u};
    if (sourceEvents.count == 0u)
        return target;

    target.offset = output.Reserve(static_cast<std::size_t>(sourceEvents.count) * kTargetEventSize);
    for (std::uint32_t i = 0u; i < sourceEvents.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceEvents.offset) +
                               static_cast<std::size_t>(i) * kSourceEventSize;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(kTargetEventSize);
        std::array<std::uint8_t, kTargetEventSize> rec{};

        // ID/data/bone/position are unchanged.
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(so), 24u, rec.begin());
        const auto timer = ConvertEventTimer(
            output,
            source,
            so + 24u,
            windows,
            sequences,
            externalBySequence);
        std::copy(timer.begin(), timer.end(), rec.begin() + 24u);
        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

} // namespace turtle335::m2
