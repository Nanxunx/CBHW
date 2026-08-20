#pragma once

#include <string>
#include <vector>

#include "turtle335/m2/particle/ParticleData.h"


namespace turtle335
{


struct ValidationIssue
{
    std::string code;

    std::string severity;

    std::string message;
};



class ParticleValidator
{
public:


    std::vector<ValidationIssue> Validate(
        const ParticleSystem& system
    );



private:


    void ValidateEmitter(
        const ParticleSystem& system,
        std::vector<ValidationIssue>& issues
    );



    void ValidateTexture(
        const ParticleSystem& system,
        std::vector<ValidationIssue>& issues
    );


    void ValidateOffset(
        const ParticleSystem& system,
        std::vector<ValidationIssue>& issues
    );


};



}