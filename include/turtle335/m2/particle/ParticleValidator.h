#pragma once

#include <string>
#include <vector>


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

    std::vector<ValidationIssue> Validate();


private:

    void ValidateEmitter(
        std::vector<ValidationIssue>& issues
    );


    void ValidateTexture(
        std::vector<ValidationIssue>& issues
    );


    void ValidateAlpha(
        std::vector<ValidationIssue>& issues
    );


    void ValidateBlendMode(
        std::vector<ValidationIssue>& issues
    );


    void ValidateAnimation(
        std::vector<ValidationIssue>& issues
    );

};


}