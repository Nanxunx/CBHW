#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d, std::size_t o, std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
}

void PutU32(std::vector<std::uint8_t>& d, std::size_t o, std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
    d[o + 2] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
    d[o + 3] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
}

std::vector<std::uint8_t> MakeM2()
{
    constexpr std::size_t seqOff = 304u;
    constexpr std::size_t ribbonOff = seqOff + 3u * 64u;
    constexpr std::size_t particleOff = ribbonOff + 176u;
    std::vector<std::uint8_t> d(particleOff + 476u, 0u);
    std::memcpy(d.data(), "MD20", 4u);
    PutU32(d, 4u, 264u);
    PutU32(d, 0x1cu, 3u); PutU32(d, 0x20u, static_cast<std::uint32_t>(seqOff));
    PutU32(d, 0x120u, 1u); PutU32(d, 0x124u, static_cast<std::uint32_t>(ribbonOff));
    PutU32(d, 0x128u, 1u); PutU32(d, 0x12cu, static_cast<std::uint32_t>(particleOff));

    PutU16(d, seqOff + 0u, 0u); PutU16(d, seqOff + 2u, 1u); PutU32(d, seqOff + 4u, 100u); PutU16(d, seqOff + 62u, 1u);
    PutU16(d, seqOff + 64u + 0u, 0u); PutU16(d, seqOff + 64u + 2u, 0u); PutU32(d, seqOff + 64u + 4u, 200u); PutU16(d, seqOff + 64u + 62u, 0u);
    PutU16(d, seqOff + 128u + 0u, 160u); PutU32(d, seqOff + 128u + 4u, 300u); PutU32(d, seqOff + 128u + 12u, 0x40u); PutU16(d, seqOff + 128u + 62u, 1u);
    return d;
}

std::vector<std::uint8_t> MakeAnimationDataDbc()
{
    constexpr std::uint32_t records = 226u;
    constexpr std::uint32_t recordSize = 32u;
    std::vector<std::uint8_t> d(20u + records * recordSize + 1u, 0u);
    std::memcpy(d.data(), "WDBC", 4u);
    PutU32(d, 4u, records); PutU32(d, 8u, 8u); PutU32(d, 12u, recordSize); PutU32(d, 16u, 1u);
    for (std::uint32_t i = 0; i < records; ++i)
    {
        const std::size_t row = 20u + static_cast<std::size_t>(i) * recordSize;
        PutU32(d, row, i); PutU32(d, row + 20u, 0u);
    }
    PutU32(d, 20u + 170u * recordSize + 20u, 19u);
    PutU32(d, 20u + 19u * recordSize + 20u, 18u);
    PutU32(d, 20u + 18u * recordSize + 20u, 17u);
    PutU32(d, 20u + 17u * recordSize + 20u, 16u);
    PutU32(d, 20u + 146u * recordSize + 20u, 148u);
    return d;
}

} // namespace

int main()
{
    using namespace turtle335::m2;
    const auto model = ParseWotlkM2(MakeM2());
    assert(model.version == 264u);
    assert(model.sequences.size() == 3u);
    assert(model.ribbons.count == 1u);
    assert(model.particles.count == 1u);
    assert(model.hasAliasSequences && model.hasSubAnimations && model.hasDuplicateAnimationIds);
    assert(ClassifyM2(model) == M2ConversionGate::RibbonWriterRequired);

    const auto windows = BuildClassicSequenceWindows(model.sequences);
    assert(windows[0].start == 3333u && windows[0].end == 3433u);
    assert(windows[1].start == 6766u && windows[1].end == 6966u);
    assert(windows[2].start == 10299u && windows[2].end == 10599u);

    const auto records = BuildClassicSequenceRecords(model.sequences);
    assert(records.size() == 3u * 68u);
    assert(records[66u] == 1u && records[67u] == 0u);
    assert(records[68u + 66u] == 0u && records[68u + 67u] == 0u);

    const auto lookup = BuildClassicAnimationLookup(model.sequences);
    assert(lookup.size() == 161u);
    assert(lookup[0] == 1);
    assert(lookup[160] == 2);

    const auto graph = ParseBuild12340AnimationFallbackGraph(MakeAnimationDataDbc());
    assert(graph[28] == 27u);
    assert(graph[108] == 111u);
    assert(graph[112] == 111u);
    assert(graph[146] == 0u && graph[172] == 16u && graph[181] == 19u);

    WotlkM2Sequence stand{}; stand.animationId = 0u;
    WotlkM2Sequence anim16{}; anim16.animationId = 16u;
    auto playable = BuildClassicPlayableAnimationLookup({stand, anim16}, graph);
    assert(playable.size() == 226u);
    assert(playable[170].fallbackAnimationId == 16);
    assert(playable[146].fallbackAnimationId == 0);

    WotlkM2Sequence anim19{}; anim19.animationId = 19u;
    playable = BuildClassicPlayableAnimationLookup({stand, anim19}, graph);
    assert(playable[170].fallbackAnimationId == 19);

    WotlkM2Sequence anim27{}; anim27.animationId = 27u;
    playable = BuildClassicPlayableAnimationLookup({stand, anim27}, graph);
    assert(playable[28].fallbackAnimationId == 27);

    WotlkM2Sequence anim111{}; anim111.animationId = 111u;
    playable = BuildClassicPlayableAnimationLookup({stand, anim111}, graph);
    assert(playable[108].fallbackAnimationId == 111);
    assert(playable[112].fallbackAnimationId == 111);

    std::cout << "PASS m2 core\n";
    return 0;
}
