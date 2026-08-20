#include "turtle335/m2/particle/ParticleValidator.h"


namespace turtle335
{


std::vector<ValidationIssue>
ParticleValidator::Validate()
{
    std::vector<ValidationIssue> issues;


    ValidateEmitter(issues);

    ValidateTexture(issues);

    ValidateAlpha(issues);

    ValidateBlendMode(issues);

    ValidateAnimation(issues);


    return issues;
}



void ParticleValidator::ValidateEmitter(
    std::vector<ValidationIssue>& issues)
{
    (void)issues;
}



void ParticleValidator::ValidateTexture(
    std::vector<ValidationIssue>& issues)
{
    (void)issues;
}



void ParticleValidator::ValidateAlpha(
    std::vector<ValidationIssue>& issues)
{
    (void)issues;
}



void ParticleValidator::ValidateBlendMode(
    std::vector<ValidationIssue>& issues)
{
    (void)issues;
}



void ParticleValidator::ValidateAnimation(
    std::vector<ValidationIssue>& issues)
{
    (void)issues;
}


}