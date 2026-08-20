#include "turtle335/m2/effect/EffectValidator.h"


namespace turtle335::m2::effect
{


void EffectValidationResult::Add(
    RiskLevel level,
    const std::string& category,
    const std::string& message)
{
    if(level == RiskLevel::Block)
    {
        passed = false;
    }


    issues.push_back(
    {
        level,
        category,
        message
    });
}



EffectValidationResult EffectValidator::Validate(
    const std::vector<uint8_t>& classicM2) const
{
    EffectValidationResult result;


    if(classicM2.empty())
    {
        result.Add(
            RiskLevel::Block,
            "M2",
            "Empty M2 payload");
    }


    return result;
}


}