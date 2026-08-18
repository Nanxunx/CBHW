#pragma once

#include <cstdint>
#include <vector>

namespace turtle335::adt {

// Normalizes one complete raw WotLK MCSH chunk (HSCM + 512-byte payload) into
// a target-compatible full-edge HSCM chunk. If source bit15 was clear, row 63
// and column 63 are materialized from 62 exactly as Noggit's build-12340 loader
// does. An all-zero normalized shadow map is omitted and returns {}.
std::vector<std::uint8_t> NormalizeWotlkMcsh(const std::vector<std::uint8_t>& rawMcsh,
                                             bool sourceDoNotFixAlphaMap);

} // namespace turtle335::adt
