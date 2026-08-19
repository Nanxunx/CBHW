#include "turtle335/m2/QuaternionCodec.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void AppendI16(std::vector<std::uint8_t>& out, const std::int16_t value)
{
    const auto u = static_cast<std::uint16_t>(value);
    out.push_back(static_cast<std::uint8_t>(u & 0xffu));
    out.push_back(static_cast<std::uint8_t>((u >> 8u) & 0xffu));
}

float ReadF32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    const std::uint32_t bits = static_cast<std::uint32_t>(d[o]) |
                               (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
                               (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
                               (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    assert(DecodeWotlkQuaternionComponent(static_cast<std::int16_t>(-1)) == 1.0f);
    assert(DecodeWotlkQuaternionComponent(static_cast<std::int16_t>(32767)) == 0.0f);
    assert(DecodeWotlkQuaternionComponent(static_cast<std::int16_t>(-32767)) == 0.0f);

    std::vector<std::uint8_t> compressed;
    AppendI16(compressed, static_cast<std::int16_t>(32767));
    AppendI16(compressed, static_cast<std::int16_t>(-32767));
    AppendI16(compressed, static_cast<std::int16_t>(-1));
    AppendI16(compressed, static_cast<std::int16_t>(1));

    const auto expanded = ExpandWotlkQuaternionKey(compressed, 1);
    assert(expanded.size() == 16u);
    assert(ReadF32(expanded, 0u) == 0.0f);
    assert(ReadF32(expanded, 4u) == 0.0f);
    assert(ReadF32(expanded, 8u) == 1.0f);
    assert(std::fabs(ReadF32(expanded, 12u) + 0.9999695f) < 0.00001f);

    std::vector<std::uint8_t> spline;
    spline.insert(spline.end(), compressed.begin(), compressed.end());
    spline.insert(spline.end(), compressed.begin(), compressed.end());
    spline.insert(spline.end(), compressed.begin(), compressed.end());
    assert(ExpandWotlkQuaternionKey(spline, 2).size() == 48u);
    assert(BuildClassicIdentityQuaternionKey(1).size() == 16u);
    assert(BuildClassicIdentityQuaternionKey(3).size() == 48u);
    assert(ReadF32(BuildClassicIdentityQuaternionKey(1), 12u) == 1.0f);

    std::cout << "PASS quaternion codec\n";
    return 0;
}
