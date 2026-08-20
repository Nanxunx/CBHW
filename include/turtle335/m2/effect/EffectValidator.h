#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace turtle335::m2::effect
{

enum class RiskLevel
{
    Safe,
    Warning,
    Block
};


struct EffectIssue
{
    RiskLevel level;

    std::string category;

    std::string message;
};


struct EffectValidationResult
{
    bool passed = true;

    std::vector<EffectIssue> issues;


    void Add(
        RiskLevel level,
        const std::string& category,
        const std::string& message);
};


class EffectValidator
{
public:

    EffectValidationResult Validate(
        const std::vector<uint8_t>& classicM2) const;

};


}