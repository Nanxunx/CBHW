#include "turtle335/m2/AuxiliaryTrackWriters.h"

#include <cassert>
#include <cstdint>
#include <cstring>
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

std::uint16_t GetU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1u]) << 8u);
}

std::uint32_t GetU32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
           (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
           (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
}

void PutF32(std::vector<std::uint8_t>& d, const std::size_t o, const float value)
{
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    PutU32(d, o, bits);
}

float GetF32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    const auto bits = GetU32(d, o);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    constexpr std::uint32_t colorOff = 32u;
    constexpr std::uint32_t transOff = 72u;
    constexpr std::uint32_t texAnimOff = 92u;
    constexpr std::uint32_t attachOff = 152u;
    std::vector<std::uint8_t> source(320u, 0u);

    // Color RGB: one outer group / one key. This test exercises the selected
    // canonical ConstantNoRanges representation. Historical successful target
    // corpora contain both this and per-sequence expanded legal forms, so the
    // core policy remains explicit rather than implicit.
    PutU16(source, colorOff + 0u, 0u);
    PutU16(source, colorOff + 2u, 0xffffu);
    PutU32(source, colorOff + 4u, 1u); PutU32(source, colorOff + 8u, 250u);
    PutU32(source, colorOff + 12u, 1u); PutU32(source, colorOff + 16u, 258u);
    PutU32(source, 250u, 1u); PutU32(source, 254u, 282u);
    PutU32(source, 258u, 1u); PutU32(source, 262u, 286u);
    PutU32(source, 282u, 7u);
    PutF32(source, 286u, 0.25f);
    PutF32(source, 290u, 0.5f);
    PutF32(source, 294u, 1.0f);
    PutU16(source, colorOff + 20u + 2u, 0xffffu);

    PutU16(source, transOff + 2u, 0xffffu);
    PutU16(source, texAnimOff + 2u, 0xffffu);
    PutU16(source, texAnimOff + 20u + 2u, 0xffffu);
    PutU16(source, texAnimOff + 40u + 2u, 0xffffu);

    // Attachment fixed prefix plus an empty WotLK enabled track. Real V4.4
    // paired Golden targets serialize this as Times=[0], Keys=[1], no ranges.
    for (std::size_t i = 0u; i < 20u; ++i)
        source[attachOff + i] = static_cast<std::uint8_t>(0x40u + i);
    PutU16(source, attachOff + 20u + 2u, 0xffffu);

    WotlkM2Sequence sequence{};
    sequence.animationId = 0u;
    sequence.length = 100u;
    sequence.flags = 0x20u;
    const std::vector<WotlkM2Sequence> sequences{sequence};
    const std::vector<ClassicSequenceWindow> windows{{3333u, 3433u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr};

    BinaryBuilder output(std::vector<std::uint8_t>(324u, 0u));
    const auto colors = ConvertWotlkColors(output, source, M2ArrayRef{1u, colorOff}, windows, sequences, sidecars);
    const auto transparency = ConvertWotlkTransparency(output, source, M2ArrayRef{1u, transOff}, windows, sequences, sidecars);
    const auto texAnims = ConvertWotlkTextureAnimations(output, source, M2ArrayRef{1u, texAnimOff}, windows, sequences, sidecars);
    const auto attachments = ConvertWotlkAttachments(output, source, M2ArrayRef{1u, attachOff}, windows, sequences, sidecars);

    assert(colors.count == 1u && colors.offset == 324u);
    const auto& d = output.Bytes();

    assert(GetU32(d, colors.offset + 4u) == 0u);
    assert(GetU32(d, colors.offset + 12u) == 1u);
    assert(GetU32(d, colors.offset + 20u) == 1u);
    const auto timeOff = GetU32(d, colors.offset + 16u);
    const auto keyOff = GetU32(d, colors.offset + 24u);
    assert(GetU32(d, timeOff) == 7u);
    assert(GetF32(d, keyOff + 0u) == 0.25f);
    assert(GetF32(d, keyOff + 4u) == 0.5f);
    assert(GetF32(d, keyOff + 8u) == 1.0f);

    assert(transparency.count == 1u);
    assert(texAnims.count == 1u);
    assert(attachments.count == 1u);
    for (std::size_t i = 0u; i < 20u; ++i)
        assert(d[attachments.offset + i] == static_cast<std::uint8_t>(0x40u + i));

    const std::size_t enabled = static_cast<std::size_t>(attachments.offset) + 20u;
    assert(GetU32(d, enabled + 4u) == 0u);   // no ranges
    assert(GetU32(d, enabled + 12u) == 1u); // one time
    assert(GetU32(d, enabled + 20u) == 1u); // one key
    const auto enabledTime = GetU32(d, enabled + 16u);
    const auto enabledKey = GetU32(d, enabled + 24u);
    assert(GetU32(d, enabledTime) == 0u);
    assert(d[enabledKey] == 1u);

    // Paired V4.6 selected Golden evidence shows that empty per-sequence Color
    // alpha and Transparency groups are opaque (32767), not zero. Exercise an
    // actual per-sequence outer array so the synthesized default is visible.
    constexpr std::uint32_t color2 = 32u;
    constexpr std::uint32_t trans2 = 72u;
    std::vector<std::uint8_t> defaultsSource(256u, 0u);
    PutU16(defaultsSource, color2 + 2u, 0xffffu);          // RGB empty
    PutU16(defaultsSource, color2 + 20u + 2u, 0xffffu);   // alpha non-global
    PutU32(defaultsSource, color2 + 20u + 4u, 2u); PutU32(defaultsSource, color2 + 20u + 8u, 120u);
    PutU32(defaultsSource, color2 + 20u + 12u, 2u); PutU32(defaultsSource, color2 + 20u + 16u, 136u);
    PutU32(defaultsSource, 120u, 0u); PutU32(defaultsSource, 124u, 0u);
    PutU32(defaultsSource, 128u, 1u); PutU32(defaultsSource, 132u, 200u);
    PutU32(defaultsSource, 136u, 0u); PutU32(defaultsSource, 140u, 0u);
    PutU32(defaultsSource, 144u, 1u); PutU32(defaultsSource, 148u, 204u);
    PutU32(defaultsSource, 200u, 5u);
    PutU16(defaultsSource, 204u, 12345u);

    PutU16(defaultsSource, trans2 + 2u, 0xffffu);
    PutU32(defaultsSource, trans2 + 4u, 2u); PutU32(defaultsSource, trans2 + 8u, 152u);
    PutU32(defaultsSource, trans2 + 12u, 2u); PutU32(defaultsSource, trans2 + 16u, 168u);
    PutU32(defaultsSource, 152u, 0u); PutU32(defaultsSource, 156u, 0u);
    PutU32(defaultsSource, 160u, 0u); PutU32(defaultsSource, 164u, 0u);
    PutU32(defaultsSource, 168u, 0u); PutU32(defaultsSource, 172u, 0u);
    PutU32(defaultsSource, 176u, 0u); PutU32(defaultsSource, 180u, 0u);

    WotlkM2Sequence sequence2 = sequence;
    sequence2.animationId = 1u;
    sequence2.length = 200u;
    const std::vector<WotlkM2Sequence> twoSequences{sequence, sequence2};
    const std::vector<ClassicSequenceWindow> twoWindows{{3333u,3433u},{6766u,6966u}};
    const std::vector<const std::vector<std::uint8_t>*> twoSidecars{nullptr,nullptr};
    BinaryBuilder defaultsOutput(std::vector<std::uint8_t>(324u, 0u));
    const auto colorsDefault = ConvertWotlkColors(
        defaultsOutput, defaultsSource, M2ArrayRef{1u,color2}, twoWindows, twoSequences, twoSidecars);
    const auto transDefault = ConvertWotlkTransparency(
        defaultsOutput, defaultsSource, M2ArrayRef{1u,trans2}, twoWindows, twoSequences, twoSidecars);
    const auto& dd = defaultsOutput.Bytes();

    const std::size_t alphaTrack = static_cast<std::size_t>(colorsDefault.offset) + 28u;
    assert(GetU32(dd, alphaTrack + 20u) == 4u);
    const auto alphaKeys = GetU32(dd, alphaTrack + 24u);
    assert(GetU16(dd, alphaKeys + 0u) == 32767u);
    assert(GetU16(dd, alphaKeys + 2u) == 32767u);
    assert(GetU16(dd, alphaKeys + 4u) == 12345u);
    assert(GetU16(dd, alphaKeys + 6u) == 12345u);

    assert(GetU32(dd, transDefault.offset + 20u) == 4u);
    const auto transKeys = GetU32(dd, transDefault.offset + 24u);
    for (std::size_t i = 0u; i < 4u; ++i)
        assert(GetU16(dd, transKeys + i * 2u) == 32767u);

    std::cout << "PASS auxiliary track writers\n";
    return 0;
}
