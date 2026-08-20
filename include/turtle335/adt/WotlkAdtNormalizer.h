#pragma once

#include "turtle335/adt/Mh2oReader.h"
#include "turtle335/adt/NormalizedAdt.h"
#include "turtle335/adt/WotlkAdtReader.h"

#include <cstddef>
#include <string>
#include <vector>

namespace turtle335::adt {

enum class WotlkAdtIssueSeverity
{
    Loss,
    Blocker
};

struct WotlkAdtNormalizationIssue
{
    WotlkAdtIssueSeverity severity = WotlkAdtIssueSeverity::Loss;
    // SIZE_MAX means a top-level/global issue rather than one MCNK.
    std::size_t cellSlot = static_cast<std::size_t>(-1);
    std::string code;
    std::string message;
};

struct WotlkAdtNormalizationResult
{
    NormalizedAdt adt;
    bool ready = true;
    bool lossless = true;
    std::vector<WotlkAdtNormalizationIssue> issues;
};

// Converts the safe structural build-12340 document into the version-neutral
// semantic model consumed by the Vanilla/Turtle writer. This stage refuses to
// alias-copy fields whose WotLK meaning differs from the target.
WotlkAdtNormalizationResult NormalizeWotlkAdt(const WotlkAdtDocument& source,
                                               const LiquidTypeResolver& liquidTypeResolver,
                                               bool sourceWdtBigAlpha = false);

} // namespace turtle335::adt
