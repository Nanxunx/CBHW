#include "turtle335/m2/particle/ParticleReader.h"

#include <cassert>


using namespace turtle335;


int main()
{
    ParticleReader reader;


    auto particles =
        reader.Read(
            2000,
            3
        );


    assert(particles.size() == 3);


    assert(particles[0].offset == 2000);

    assert(particles[1].offset == 2476);

    assert(particles[2].offset == 2952);


    assert(particles[0].size == 476);


    return 0;
}