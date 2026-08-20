#pragma once

#include "turtle335/adt/LegacyLiquid.h"
#include "turtle335/adt/Mh2oReader.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace turtle335::dbc {

struct LiquidTypeRecord
{
    std::uint32_t id = 0;
    std::uint32_t soundBank = 0;
    adt::LiquidCategory category = adt::LiquidCategory::Unknown;
};

class LiquidTypeDbc
{
public:
    adt::LiquidCategory Resolve(std::uint16_t sourceLiquidType) const noexcept;
    const std::unordered_map<std::uint32_t, LiquidTypeRecord>& Records() const noexcept { return records_; }

private:
    friend LiquidTypeDbc ParseLiquidTypeDbc(const std::vector<std::uint8_t>& bytes);
    std::unordered_map<std::uint32_t, LiquidTypeRecord> records_;
};

// Parses the build-12340 WDBC form used by Trinity's map extractor. Only the
// fields needed for MH2O classification are interpreted:
//   field 0 = LiquidType ID
//   field 3 = SoundBank (0 water, 1 ocean, 2 magma, 3 slime)
LiquidTypeDbc ParseLiquidTypeDbc(const std::vector<std::uint8_t>& bytes);

// Convenience resolver suitable for ParseMh2oChunk / NormalizeWotlkAdt.
adt::LiquidTypeResolver MakeLiquidTypeResolver(LiquidTypeDbc table);

} // namespace turtle335::dbc
