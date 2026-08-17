#include "turtle335/adt/Mcal.h"

#include <algorithm>

namespace turtle335::adt {

std::uint8_t Expand4To8(std::uint8_t nibble) noexcept
{
    nibble &= 0x0F;
    return static_cast<std::uint8_t>(nibble * 17u);
}

std::uint8_t Quantize8To4(std::uint8_t alpha) noexcept
{
    const unsigned q = (static_cast<unsigned>(alpha) + 8u) / 17u;
    return static_cast<std::uint8_t>(std::min(q, 15u));
}

Alpha8 DecodeLegacyAlpha4(const std::uint8_t* data, std::size_t size)
{
    if (!data || size != 2048)
        throw std::invalid_argument("legacy MCAL alpha must be exactly 2048 bytes");

    Alpha8 out{};
    for (std::size_t i = 0; i < 2048; ++i)
    {
        out[i * 2]     = Expand4To8(data[i] & 0x0F);
        out[i * 2 + 1] = Expand4To8(data[i] >> 4);
    }
    return out;
}

Alpha8 DecodeBigAlpha8(const std::uint8_t* data, std::size_t size)
{
    if (!data || size != 4096)
        throw std::invalid_argument("big MCAL alpha must be exactly 4096 bytes");

    Alpha8 out{};
    std::copy_n(data, out.size(), out.begin());
    return out;
}

Alpha8 DecodeRleAlpha8(const std::uint8_t* data, std::size_t size)
{
    if (!data)
        throw std::invalid_argument("null RLE alpha input");

    Alpha8 out{};
    std::size_t in = 0;
    std::size_t outPos = 0;

    while (outPos < out.size())
    {
        if (in >= size)
            throw std::runtime_error("truncated RLE MCAL stream");

        const std::uint8_t control = data[in++];
        const std::size_t count = control & 0x7Fu;
        const bool fill = (control & 0x80u) != 0;

        if (count == 0)
            throw std::runtime_error("zero-length RLE MCAL run");
        if (outPos + count > out.size())
            throw std::runtime_error("RLE MCAL run exceeds 4096 decoded bytes");

        if (fill)
        {
            if (in >= size)
                throw std::runtime_error("truncated RLE MCAL fill run");
            const std::uint8_t value = data[in++];
            std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(outPos), count, value);
            outPos += count;
        }
        else
        {
            if (in + count > size)
                throw std::runtime_error("truncated RLE MCAL literal run");
            std::copy_n(data + in, count, out.begin() + static_cast<std::ptrdiff_t>(outPos));
            in += count;
            outPos += count;
        }
    }

    return out;
}

Alpha4Packed EncodeLegacyAlpha4(const Alpha8& alpha) noexcept
{
    Alpha4Packed out{};
    for (std::size_t i = 0; i < alpha.size(); i += 2)
    {
        const std::uint8_t lo = Quantize8To4(alpha[i]);
        const std::uint8_t hi = Quantize8To4(alpha[i + 1]);
        out[i / 2] = static_cast<std::uint8_t>(lo | (hi << 4));
    }
    return out;
}

} // namespace turtle335::adt
