#include "turtle335/m2/particle/ParticleValidator.h"

#include <cassert>


int main()
{

    turtle335::ParticleValidator validator;


    auto result = validator.Validate();


    assert(result.empty());


    return 0;
}