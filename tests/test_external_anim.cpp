#include "turtle335/m2/ExternalAnim.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
}

void PutU32(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
    d[o + 2u] = static_cast<std::uint8_t>((v >> 16u) & 0xffu);
    d[o + 3u] = static_cast<std::uint8_t>((v >> 24u) & 0xffu);
}

void PutF32(std::vector<std::uint8_t>& d, const std::size_t o, const float v)
{
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(bits));
    PutU32(d, o, bits);
}

float GetF32(const std::vector<std::uint8_t>& d)
{
    assert(d.size() == 4u);
    std::uint32_t bits = static_cast<std::uint32_t>(d[0]) |
                         (static_cast<std::uint32_t>(d[1]) << 8u) |
                         (static_cast<std::uint32_t>(d[2]) << 16u) |
                         (static_cast<std::uint32_t>(d[3]) << 24u);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    WotlkM2Sequence embedded{};
    embedded.animationId = 5u;
    embedded.subAnimationId = 0u;
    embedded.flags = 0x20u;

    WotlkM2Sequence external{};
    external.animationId = 69u;
    external.subAnimationId = 1u;
    external.flags = 0u;

    assert(!SequenceUsesExternalAnimSidecar(embedded));
    assert(SequenceUsesExternalAnimSidecar(external));
    assert(BuildWotlkAnimSidecarFilename("FelReaver", external) == "FelReaver0069-01.anim");

    // Track header at 0. Two outer groups. The inner ArrayRefs live in the
    // main M2. Group 0 payload is embedded at 100/104. Group 1 payload uses
    // offsets 0/8 into the external .anim sidecar. Offset zero is legal for
    // sidecar-relative payloads and occurs in real Build 12340 models.
    std::vector<std::uint8_t> mainM2(128u, 0u);
    PutU16(mainM2, 0u, 1u);       // interpolation
    PutU16(mainM2, 2u, 0xffffu);  // global sequence = -1
    PutU32(mainM2, 4u, 2u); PutU32(mainM2, 8u, 20u);
    PutU32(mainM2, 12u, 2u); PutU32(mainM2, 16u, 36u);

    PutU32(mainM2, 20u, 1u); PutU32(mainM2, 24u, 100u);
    PutU32(mainM2, 28u, 1u); PutU32(mainM2, 32u, 0u);
    PutU32(mainM2, 36u, 1u); PutU32(mainM2, 40u, 104u);
    PutU32(mainM2, 44u, 1u); PutU32(mainM2, 48u, 8u);

    PutU32(mainM2, 100u, 11u);
    PutF32(mainM2, 104u, 1.25f);

    std::vector<std::uint8_t> sidecar(16u, 0u);
    PutU32(sidecar, 0u, 22u);
    PutF32(sidecar, 8u, 2.5f);

    const std::vector<WotlkM2Sequence> sequences{embedded, external};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr, &sidecar};
    const auto track = ParseWotlkTrackWithExternal(mainM2, 0u, 4u, sequences, sidecars);

    assert(track.timestamps.size() == 2u);
    assert(track.timestamps[0].size() == 1u && track.timestamps[0][0] == 11u);
    assert(track.timestamps[1].size() == 1u && track.timestamps[1][0] == 22u);
    assert(track.keys[0].size() == 1u && GetF32(track.keys[0][0]) == 1.25f);
    assert(track.keys[1].size() == 1u && GetF32(track.keys[1][0]) == 2.5f);

    bool missingSidecarRejected = false;
    try
    {
        const std::vector<const std::vector<std::uint8_t>*> missing{nullptr, nullptr};
        (void)ParseWotlkTrackWithExternal(mainM2, 0u, 4u, sequences, missing);
    }
    catch (const std::runtime_error&)
    {
        missingSidecarRejected = true;
    }
    assert(missingSidecarRejected);

    // Alias sequences borrow payload storage from Sequence.Index. The alias
    // itself intentionally has no sidecar; its target does.
    WotlkM2Sequence alias{};
    alias.animationId = 136u;
    alias.flags = 0x40u;
    alias.index = 1u;
    const std::vector<WotlkM2Sequence> aliasSequences{alias, external};
    assert(ResolveWotlkPayloadSequenceIndex(aliasSequences, 0u) == 1u);

    std::vector<std::uint8_t> aliasMain(96u, 0u);
    PutU16(aliasMain, 0u, 1u);
    PutU16(aliasMain, 2u, 0xffffu);
    PutU32(aliasMain, 4u, 2u); PutU32(aliasMain, 8u, 20u);
    PutU32(aliasMain, 12u, 2u); PutU32(aliasMain, 16u, 36u);
    PutU32(aliasMain, 20u, 1u); PutU32(aliasMain, 24u, 0u);
    PutU32(aliasMain, 28u, 0u); PutU32(aliasMain, 32u, 0u);
    PutU32(aliasMain, 36u, 1u); PutU32(aliasMain, 40u, 8u);
    PutU32(aliasMain, 44u, 0u); PutU32(aliasMain, 48u, 0u);
    const std::vector<const std::vector<std::uint8_t>*> aliasSidecars{nullptr, &sidecar};
    const auto aliasTrack = ParseWotlkTrackWithExternal(
        aliasMain, 0u, 4u, aliasSequences, aliasSidecars);
    assert(aliasTrack.timestamps[0].size() == 1u && aliasTrack.timestamps[0][0] == 22u);
    assert(aliasTrack.keys[0].size() == 1u && GetF32(aliasTrack.keys[0][0]) == 2.5f);

    // Offset zero remains invalid for a non-empty payload stored in the main M2.
    bool mainZeroRejected = false;
    try
    {
        std::vector<std::uint8_t> invalidMain(80u, 0u);
        PutU16(invalidMain, 2u, 0xffffu);
        PutU32(invalidMain, 4u, 1u); PutU32(invalidMain, 8u, 20u);
        PutU32(invalidMain, 12u, 1u); PutU32(invalidMain, 16u, 28u);
        PutU32(invalidMain, 20u, 1u); PutU32(invalidMain, 24u, 0u);
        PutU32(invalidMain, 28u, 1u); PutU32(invalidMain, 32u, 60u);
        PutF32(invalidMain, 60u, 1.0f);
        const std::vector<WotlkM2Sequence> oneEmbedded{embedded};
        const std::vector<const std::vector<std::uint8_t>*> noSidecar{nullptr};
        (void)ParseWotlkTrackWithExternal(invalidMain, 0u, 4u, oneEmbedded, noSidecar);
    }
    catch (const std::runtime_error&)
    {
        mainZeroRejected = true;
    }
    assert(mainZeroRejected);

    std::cout << "PASS external anim resolver\n";
    return 0;
}
