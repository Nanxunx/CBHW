#include "turtle335/m2/particle/ParticleValidator.h"



namespace turtle335
{


std::vector<ValidationIssue>
ParticleValidator::Validate(
    const ParticleSystem& system
)
{
    std::vector<ValidationIssue> issues;


    ValidateEmitter(
        system,
        issues
    );


    ValidateTexture(
        system,
        issues
    );


    ValidateOffset(
        system,
        issues
    );


    return issues;
}




void ParticleValidator::ValidateEmitter(
    const ParticleSystem& system,
    std::vector<ValidationIssue>& issues
)
{

    if(system.count != system.emitters.size())
    {
        issues.push_back(
        {
            "PARTICLE_COUNT_MISMATCH",
            "ERROR",
            "Particle emitter count mismatch"
        });
    }

}




void ParticleValidator::ValidateTexture(
    const ParticleSystem& system,
    std::vector<ValidationIssue>& issues
)
{

    for(const auto& emitter : system.emitters)
    {

        if(emitter.textureId == 0)
        {

            issues.push_back(
            {
                "MISSING_TEXTURE",
                "WARNING",
                "Particle texture is missing"
            });

        }

    }

}





void ParticleValidator::ValidateOffset(
    const ParticleSystem& system,
    std::vector<ValidationIssue>& issues
)
{

    for(const auto& emitter : system.emitters)
    {

        if(emitter.offset % 476 != 0)
        {

            issues.push_back(
            {
                "INVALID_PARTICLE_OFFSET",
                "ERROR",
                "Particle emitter offset is not aligned"
            });

        }

    }

}
}
