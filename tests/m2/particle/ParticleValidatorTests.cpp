#include "turtle335/m2/particle/ParticleValidator.h"

#include <cassert>


using namespace turtle335;



int main()
{

    ParticleSystem system;


    system.count = 1;


    ParticleEmitter emitter;

    emitter.index = 0;

    emitter.offset = 0;

    emitter.textureId = 1;


    system.emitters.push_back(
        emitter
    );



    ParticleValidator validator;


    auto result =
        validator.Validate(
            system
        );


    assert(
        result.empty()
    );



    system.count = 2;


    result =
        validator.Validate(
            system
        );


    assert(
        !result.empty()
    );


    return 0;
}