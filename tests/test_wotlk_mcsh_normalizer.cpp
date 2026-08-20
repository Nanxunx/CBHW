#include "turtle335/adt/WotlkMcshNormalizer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace turtle335::adt;

static void SetBit(std::vector<std::uint8_t>& raw, std::size_t x, std::size_t y, bool value)
{
    const std::size_t bit = y * 64u + x;
    const std::size_t at = 8u + bit / 8u;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (bit % 8u));
    if (value)
        raw[at] |= mask;
    else
        raw[at] &= static_cast<std::uint8_t>(~mask);
}

static bool GetBit(const std::vector<std::uint8_t>& raw, std::size_t x, std::size_t y)
{
    const std::size_t bit = y * 64u + x;
    return ((raw[8u + bit / 8u] >> (bit % 8u)) & 1u) != 0;
}

static std::vector<std::uint8_t> MakeShadow()
{
    std::vector<std::uint8_t> raw(8u + 512u, 0);
    std::memcpy(raw.data(), "HSCM", 4);
    raw[4] = 0x00;
    raw[5] = 0x02;
    return raw;
}

int main()
{
    {
        auto source = MakeShadow();
        SetBit(source, 62, 10, true);
        SetBit(source, 10, 62, true);
        SetBit(source, 62, 62, true);
        SetBit(source, 63, 10, false);
        SetBit(source, 10, 63, false);
        SetBit(source, 63, 63, false);

        const auto target = NormalizeWotlkMcsh(source, false);
        assert(target.size() == 520u);
        assert(GetBit(target, 62, 10));
        assert(GetBit(target, 63, 10));
        assert(GetBit(target, 10, 62));
        assert(GetBit(target, 10, 63));
        assert(GetBit(target, 63, 63));
    }

    {
        auto source = MakeShadow();
        SetBit(source, 62, 10, true);
        SetBit(source, 63, 10, false);
        const auto target = NormalizeWotlkMcsh(source, true);
        assert(GetBit(target, 62, 10));
        assert(!GetBit(target, 63, 10)); // source declared full edges, so do not synthesize
    }

    {
        const auto empty = NormalizeWotlkMcsh(MakeShadow(), false);
        assert(empty.empty());
    }

    {
        auto malformed = MakeShadow();
        malformed[4] = 0xFF;
        bool rejected = false;
        try { (void)NormalizeWotlkMcsh(malformed, false); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }

    std::cout << "turtle335_wotlk_mcsh_normalizer_tests: OK\n";
    return 0;
}
