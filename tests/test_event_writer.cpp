#include "turtle335/m2/EventWriter.h"

#include <cassert>
#include <cstdint>
#include <iostream>
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

std::uint32_t GetU32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
           (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
           (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    constexpr std::uint32_t eventOff = 32u;
    std::vector<std::uint8_t> source(160u, 0u);

    source[eventOff + 0u] = '$';
    source[eventOff + 1u] = 'H';
    source[eventOff + 2u] = 'I';
    source[eventOff + 3u] = 'T';
    PutU32(source, eventOff + 4u, 7u);
    PutU32(source, eventOff + 8u, 3u);
    for (std::size_t i = 0u; i < 12u; ++i)
        source[eventOff + 12u + i] = static_cast<std::uint8_t>(0x20u + i);

    // Two per-sequence event time groups. First empty, second has two events
    // whose payload is in an external .anim sidecar.
    PutU16(source, eventOff + 24u + 0u, 0u);
    PutU16(source, eventOff + 24u + 2u, 0xffffu);
    PutU32(source, eventOff + 24u + 4u, 2u);
    PutU32(source, eventOff + 24u + 8u, 80u);
    PutU32(source, 80u, 0u); PutU32(source, 84u, 0u);
    PutU32(source, 88u, 2u); PutU32(source, 92u, 4u);

    std::vector<std::uint8_t> sidecar(16u, 0u);
    PutU32(sidecar, 4u, 10u);
    PutU32(sidecar, 8u, 20u);

    WotlkM2Sequence embedded{};
    embedded.animationId = 0u;
    embedded.length = 100u;
    embedded.flags = 0x20u;
    WotlkM2Sequence external{};
    external.animationId = 69u;
    external.length = 200u;
    external.flags = 0u;
    const std::vector<WotlkM2Sequence> sequences{embedded, external};
    const std::vector<ClassicSequenceWindow> windows{{3333u, 3433u}, {6766u, 6966u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr, &sidecar};

    BinaryBuilder output(std::vector<std::uint8_t>(324u, 0u));
    const auto events = ConvertWotlkEvents(
        output,
        source,
        M2ArrayRef{1u, eventOff},
        windows,
        sequences,
        sidecars);

    assert(events.count == 1u && events.offset == 324u);
    const auto& d = output.Bytes();
    const std::size_t e = events.offset;
    assert(d[e + 0u] == '$' && d[e + 1u] == 'H' && d[e + 2u] == 'I' && d[e + 3u] == 'T');
    assert(GetU32(d, e + 4u) == 7u);
    assert(GetU32(d, e + 8u) == 3u);

    const std::size_t timer = e + 24u;
    assert(GetU32(d, timer + 4u) == 3u); // 2 sequence ranges + [0,0]
    assert(GetU32(d, timer + 12u) == 2u);
    const auto rangeOff = GetU32(d, timer + 8u);
    assert(GetU32(d, rangeOff + 0u) == 0u && GetU32(d, rangeOff + 4u) == 0u);
    assert(GetU32(d, rangeOff + 8u) == 0u && GetU32(d, rangeOff + 12u) == 2u);
    assert(GetU32(d, rangeOff + 16u) == 0u && GetU32(d, rangeOff + 20u) == 0u);
    const auto timeOff = GetU32(d, timer + 16u);
    assert(GetU32(d, timeOff + 0u) == 6776u);
    assert(GetU32(d, timeOff + 4u) == 6786u);

    std::cout << "PASS event writer\n";
    return 0;
}
