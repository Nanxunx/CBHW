#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace turtle335::adt {

struct AdtValidationReport
{
    std::size_t fileSize = 0;
    std::uint32_t version = 0;
    std::size_t textureCount = 0;
    std::size_t m2PathCount = 0;
    std::size_t wmoPathCount = 0;
    std::size_t m2PlacementCount = 0;
    std::size_t wmoPlacementCount = 0;
    std::size_t mcnkCount = 0;
    std::size_t liquidRecordCount = 0;
    std::size_t soundEmitterCount = 0;
};

AdtValidationReport ValidateVanillaAdt(const std::vector<std::uint8_t>& bytes);

} // namespace turtle335::adt
