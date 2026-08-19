#include "turtle335/m2/AnimationMetadata.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace turtle335::m2 {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, const std::size_t offset, const char* what)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
        throw std::runtime_error(std::string(what) + " is outside bytes");
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void WriteU16(std::vector<std::uint8_t>& out, const std::size_t offset, const std::uint16_t value)
{
    out[offset] = static_cast<std::uint8_t>(value & 0xffu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}

void WriteU32(std::vector<std::uint8_t>& out, const std::size_t offset, const std::uint32_t value)
{
    out[offset] = static_cast<std::uint8_t>(value & 0xffu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

std::uint16_t ResolveFallback(
    const std::uint16_t requested,
    const std::unordered_set<std::uint16_t>& present,
    const AnimationFallbackGraph& graph)
{
    std::uint16_t current = requested;
    std::unordered_set<std::uint16_t> seen;
    while (present.find(current) == present.end())
    {
        if (!seen.insert(current).second)
            return 0u;
        if (current >= graph.size())
            return 0u;
        current = graph[current];
    }
    return current;
}

} // namespace

std::vector<ClassicSequenceWindow> BuildClassicSequenceWindows(
    const std::vector<WotlkM2Sequence>& sequences,
    const std::uint32_t gap)
{
    std::uint64_t timeline = 0u;
    std::vector<ClassicSequenceWindow> result;
    result.reserve(sequences.size());
    for (const auto& sequence : sequences)
    {
        timeline += gap;
        const std::uint64_t start = timeline;
        timeline += sequence.length;
        if (timeline > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("Classic sequence timeline exceeds uint32");
        result.push_back({static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(timeline)});
    }
    return result;
}

std::vector<std::uint8_t> BuildClassicSequenceRecords(
    const std::vector<WotlkM2Sequence>& sequences,
    const std::uint32_t gap)
{
    constexpr std::size_t stride = 68u;
    const auto windows = BuildClassicSequenceWindows(sequences, gap);
    std::vector<std::uint8_t> out(sequences.size() * stride, 0u);
    for (std::size_t i = 0; i < sequences.size(); ++i)
    {
        const std::size_t at = i * stride;
        WriteU16(out, at + 0u, sequences[i].animationId);
        WriteU16(out, at + 2u, sequences[i].subAnimationId);
        WriteU32(out, at + 4u, windows[i].start);
        WriteU32(out, at + 8u, windows[i].end);
        std::copy(sequences[i].tailFromMoveSpeed.begin(), sequences[i].tailFromMoveSpeed.end(),
                  out.begin() + static_cast<std::ptrdiff_t>(at + 12u));
    }
    return out;
}

std::vector<std::int16_t> BuildClassicAnimationLookup(const std::vector<WotlkM2Sequence>& sequences)
{
    if (sequences.empty())
        return {};
    std::uint16_t maxId = 0u;
    for (const auto& sequence : sequences)
        maxId = std::max(maxId, sequence.animationId);
    std::vector<std::int16_t> lookup(static_cast<std::size_t>(maxId) + 1u, -1);

    for (std::size_t i = 0; i < sequences.size(); ++i)
    {
        const auto id = sequences[i].animationId;
        if (sequences[i].subAnimationId == 0u && lookup[id] < 0)
        {
            if (i > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()))
                throw std::overflow_error("animation sequence index exceeds int16");
            lookup[id] = static_cast<std::int16_t>(i);
        }
    }
    for (std::size_t i = 0; i < sequences.size(); ++i)
    {
        const auto id = sequences[i].animationId;
        if (lookup[id] < 0)
        {
            if (i > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()))
                throw std::overflow_error("animation sequence index exceeds int16");
            lookup[id] = static_cast<std::int16_t>(i);
        }
    }
    return lookup;
}

AnimationFallbackGraph ParseBuild12340AnimationFallbackGraph(const std::vector<std::uint8_t>& dbcBytes)
{
    if (dbcBytes.size() < 20u || std::memcmp(dbcBytes.data(), "WDBC", 4u) != 0)
        throw std::runtime_error("AnimationData.dbc is not WDBC");
    const std::uint32_t records = ReadU32(dbcBytes, 4u, "DBC record count");
    const std::uint32_t fields = ReadU32(dbcBytes, 8u, "DBC field count");
    const std::uint32_t recordSize = ReadU32(dbcBytes, 12u, "DBC record size");
    const std::uint32_t stringSize = ReadU32(dbcBytes, 16u, "DBC string size");
    if (fields != 8u || recordSize != 32u)
        throw std::runtime_error("build12340 AnimationData.dbc must be 8 fields / 32-byte records");
    const std::size_t recordBytes = static_cast<std::size_t>(records) * recordSize;
    if (20u + recordBytes > dbcBytes.size() || static_cast<std::size_t>(stringSize) > dbcBytes.size() - (20u + recordBytes))
        throw std::runtime_error("AnimationData.dbc is truncated");

    AnimationFallbackGraph graph{};
    graph.fill(0u);
    for (std::uint32_t i = 0; i < records; ++i)
    {
        const std::size_t row = 20u + static_cast<std::size_t>(i) * recordSize;
        const std::uint32_t id = ReadU32(dbcBytes, row + 0u, "AnimationData ID");
        const std::uint32_t fallback = ReadU32(dbcBytes, row + 5u * 4u, "AnimationData field 5");
        if (id < graph.size())
            graph[id] = fallback < graph.size() ? static_cast<std::uint16_t>(fallback) : 0u;
    }

    graph[121] = 14u;
    graph[146] = 0u;
    graph[172] = 16u;
    graph[174] = 16u;
    graph[181] = 19u;
    graph[191] = 159u;
    return graph;
}

std::vector<PlayableAnimationRecord> BuildClassicPlayableAnimationLookup(
    const std::vector<WotlkM2Sequence>& sequences,
    const AnimationFallbackGraph& graph)
{
    std::unordered_set<std::uint16_t> present;
    for (const auto& sequence : sequences)
        present.insert(sequence.animationId);

    std::vector<PlayableAnimationRecord> result;
    result.reserve(graph.size());
    for (std::uint16_t requested = 0u; requested < graph.size(); ++requested)
    {
        const std::uint16_t real = ResolveFallback(requested, present, graph);
        std::int16_t flags = 0;
        if (real != requested)
        {
            switch (requested)
            {
            case 6u: case 97u: case 100u: case 115u: case 123u: case 132u: case 188u:
                flags = 3; break;
            case 13u: case 45u: case 101u: case 189u:
                flags = 1; break;
            default:
                flags = 0; break;
            }
        }
        result.push_back({static_cast<std::int16_t>(real), flags});
    }
    return result;
}

} // namespace turtle335::m2
