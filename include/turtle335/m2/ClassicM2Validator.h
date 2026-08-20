#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace turtle335::m2 {

struct ClassicM2ValidationIssue
{
    std::string code;
    std::string detail;
};

struct ClassicM2ValidationResult
{
    bool valid = false;
    std::vector<ClassicM2ValidationIssue> issues;
};

// Strict structural validation for blocks whose Classic/Turtle v256 sizes are
// already Golden-verified by this project. It intentionally does not guess
// unknown/version-ambiguous structures.
ClassicM2ValidationResult ValidateClassicM2(
    const std::vector<std::uint8_t>& bytes);

} // namespace turtle335::m2
