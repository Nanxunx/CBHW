#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace turtle335::m2 {

class BinaryBuilder
{
public:
    explicit BinaryBuilder(std::vector<std::uint8_t> initial = {}) : bytes_(std::move(initial)) {}

    std::uint32_t Reserve(const std::size_t size)
    {
        const auto offset = CheckedOffset(bytes_.size());
        bytes_.resize(bytes_.size() + size, 0u);
        return offset;
    }

    std::uint32_t Append(const std::uint8_t* data, const std::size_t size)
    {
        if (size == 0u)
            return 0u;
        const auto offset = CheckedOffset(bytes_.size());
        bytes_.insert(bytes_.end(), data, data + size);
        return offset;
    }

    std::uint32_t Append(const std::vector<std::uint8_t>& data)
    {
        return data.empty() ? 0u : Append(data.data(), data.size());
    }

    void Patch(const std::uint32_t offset, const std::uint8_t* data, const std::size_t size)
    {
        const std::size_t at = static_cast<std::size_t>(offset);
        if (at > bytes_.size() || size > bytes_.size() - at)
            throw std::out_of_range("BinaryBuilder patch outside buffer");
        std::copy(data, data + size, bytes_.begin() + static_cast<std::ptrdiff_t>(at));
    }

    const std::vector<std::uint8_t>& Bytes() const noexcept { return bytes_; }
    std::vector<std::uint8_t>& Bytes() noexcept { return bytes_; }

private:
    static std::uint32_t CheckedOffset(const std::size_t value)
    {
        if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
            throw std::overflow_error("M2 output offset exceeds uint32");
        return static_cast<std::uint32_t>(value);
    }

    std::vector<std::uint8_t> bytes_;
};

} // namespace turtle335::m2
