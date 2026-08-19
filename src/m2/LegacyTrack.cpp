#include "turtle335/m2/LegacyTrack.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace turtle335::m2 {
namespace {

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    if (o > d.size() || 2u > d.size() - o)
        throw std::runtime_error(std::string(what) + " outside source M2");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1]) << 8);
}

std::int16_t ReadI16(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    return static_cast<std::int16_t>(ReadU16(d, o, what));
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    if (o > d.size() || 4u > d.size() - o)
        throw std::runtime_error(std::string(what) + " outside source M2");
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1]) << 8) |
           (static_cast<std::uint32_t>(d[o + 2]) << 16) |
           (static_cast<std::uint32_t>(d[o + 3]) << 24);
}

void PutU16(std::uint8_t* d, const std::size_t o, const std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
}

void PutU32(std::uint8_t* d, const std::size_t o, const std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
    d[o + 2] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
    d[o + 3] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
}

std::pair<std::uint32_t, std::uint32_t> ReadPair(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    return {ReadU32(d, o, what), ReadU32(d, o + 4u, what)};
}

std::uint32_t AppendRanges(BinaryBuilder& b, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& ranges)
{
    if (ranges.empty()) return 0u;
    std::vector<std::uint8_t> raw(ranges.size() * 8u, 0u);
    for (std::size_t i = 0; i < ranges.size(); ++i)
    {
        PutU32(raw.data(), i * 8u, ranges[i].first);
        PutU32(raw.data(), i * 8u + 4u, ranges[i].second);
    }
    return b.Append(raw);
}

std::uint32_t AppendTimes(BinaryBuilder& b, const std::vector<std::uint32_t>& times)
{
    if (times.empty()) return 0u;
    std::vector<std::uint8_t> raw(times.size() * 4u, 0u);
    for (std::size_t i = 0; i < times.size(); ++i) PutU32(raw.data(), i * 4u, times[i]);
    return b.Append(raw);
}

std::uint32_t AppendKeys(BinaryBuilder& b, const std::vector<std::vector<std::uint8_t>>& keys)
{
    if (keys.empty()) return 0u;
    std::vector<std::uint8_t> raw;
    std::size_t size = 0u;
    for (const auto& k : keys) size += k.size();
    raw.reserve(size);
    for (const auto& k : keys) raw.insert(raw.end(), k.begin(), k.end());
    return b.Append(raw);
}

} // namespace

WotlkTrackData ParseWotlkTrack(const std::vector<std::uint8_t>& source, const std::size_t offset, const std::size_t keySize)
{
    if (offset > source.size() || 20u > source.size() - offset)
        throw std::runtime_error("WotLK track header outside source M2");

    WotlkTrackData result;
    result.interpolationType = ReadI16(source, offset + 0u, "track interpolation");
    result.globalSequence = ReadI16(source, offset + 2u, "track global sequence");
    const auto timeOuter = ReadPair(source, offset + 4u, "track times outer");
    const auto keyOuter = ReadPair(source, offset + 12u, "track keys outer");
    if (timeOuter.first != keyOuter.first)
        throw std::runtime_error("WotLK track outer time/key count mismatch");

    result.timestamps.resize(timeOuter.first);
    result.keys.resize(keyOuter.first);
    for (std::size_t i = 0; i < timeOuter.first; ++i)
    {
        const auto times = ReadPair(source, static_cast<std::size_t>(timeOuter.second) + i * 8u, "track time group");
        const auto keys = ReadPair(source, static_cast<std::size_t>(keyOuter.second) + i * 8u, "track key group");
        if (times.first != keys.first)
            throw std::runtime_error("WotLK track inner time/key count mismatch");
        if (times.first != 0u)
        {
            const std::size_t timeBytes = static_cast<std::size_t>(times.first) * 4u;
            if (times.second == 0u || static_cast<std::size_t>(times.second) > source.size() || timeBytes > source.size() - times.second)
                throw std::runtime_error("WotLK track timestamp payload outside source M2");
            result.timestamps[i].reserve(times.first);
            for (std::size_t j = 0; j < times.first; ++j)
                result.timestamps[i].push_back(ReadU32(source, static_cast<std::size_t>(times.second) + j * 4u, "track timestamp"));
        }
        if (keys.first != 0u)
        {
            const std::size_t keyBytes = static_cast<std::size_t>(keys.first) * keySize;
            if (keys.second == 0u || static_cast<std::size_t>(keys.second) > source.size() || keyBytes > source.size() - keys.second)
                throw std::runtime_error("WotLK track key payload outside source M2");
            result.keys[i].reserve(keys.first);
            for (std::size_t j = 0; j < keys.first; ++j)
            {
                const auto begin = source.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(keys.second) + j * keySize);
                result.keys[i].emplace_back(begin, begin + static_cast<std::ptrdiff_t>(keySize));
            }
        }
    }
    return result;
}

FlattenedLegacyTrack FlattenLegacyValueTrack(const WotlkTrackData& track, const std::vector<ClassicSequenceWindow>& windows, const std::vector<std::uint8_t>& defaultKey)
{
    if (track.timestamps.size() != track.keys.size())
        throw std::runtime_error("track outer timestamp/key size mismatch");
    FlattenedLegacyTrack out;
    if (track.timestamps.empty()) return out;

    if (track.globalSequence >= 0)
    {
        if (track.timestamps.size() != 1u || track.timestamps[0].size() != track.keys[0].size())
            throw std::runtime_error("global-sequence track shape is invalid");
        out.timestamps = track.timestamps[0]; out.keys = track.keys[0]; return out;
    }

    if (track.timestamps.size() == windows.size())
    {
        std::uint32_t cursor = 0u;
        out.ranges.reserve(windows.size() + 1u);
        for (std::size_t i = 0; i < windows.size(); ++i)
        {
            const auto& ts = track.timestamps[i]; const auto& ks = track.keys[i];
            if (ts.size() != ks.size()) throw std::runtime_error("per-sequence track inner mismatch");
            const auto start = cursor;
            if (ts.size() <= 1u) cursor += 1u; else cursor += static_cast<std::uint32_t>(ts.size() - 1u);
            out.ranges.emplace_back(start, cursor); cursor += 1u;
            if (ts.empty())
            {
                out.timestamps.push_back(windows[i].start); out.timestamps.push_back(windows[i].end);
                out.keys.push_back(defaultKey); out.keys.push_back(defaultKey);
            }
            else if (ts.size() == 1u)
            {
                out.timestamps.push_back(windows[i].start + ts[0]); out.timestamps.push_back(windows[i].end + ts[0]);
                out.keys.push_back(ks[0]); out.keys.push_back(ks[0]);
            }
            else
            {
                for (std::size_t j = 0; j < ts.size(); ++j)
                {
                    out.timestamps.push_back(windows[i].start + ts[j]); out.keys.push_back(ks[j]);
                }
            }
        }
        out.ranges.emplace_back(0u, 0u); return out;
    }

    if (track.timestamps.size() == 1u)
    {
        if (track.timestamps[0].size() != track.keys[0].size()) throw std::runtime_error("single-group legacy track mismatch");
        out.timestamps = track.timestamps[0]; out.keys = track.keys[0]; return out;
    }
    throw std::runtime_error("legacy track outer count is neither sequence count nor one");
}

std::array<std::uint8_t, 28> SerializeClassicTrack(BinaryBuilder& builder, const WotlkTrackData& sourceTrack, const FlattenedLegacyTrack& track)
{
    std::array<std::uint8_t, 28> rec{};
    PutU16(rec.data(), 0u, static_cast<std::uint16_t>(sourceTrack.interpolationType));
    PutU16(rec.data(), 2u, static_cast<std::uint16_t>(sourceTrack.globalSequence));
    PutU32(rec.data(), 4u, static_cast<std::uint32_t>(track.ranges.size()));
    PutU32(rec.data(), 8u, AppendRanges(builder, track.ranges));
    PutU32(rec.data(), 12u, static_cast<std::uint32_t>(track.timestamps.size()));
    PutU32(rec.data(), 16u, AppendTimes(builder, track.timestamps));
    PutU32(rec.data(), 20u, static_cast<std::uint32_t>(track.keys.size()));
    PutU32(rec.data(), 24u, AppendKeys(builder, track.keys));
    return rec;
}

} // namespace turtle335::m2
