#include "turtle335/m2/particle/ParticleProbe.h"

#include <cassert>


using namespace turtle335;


int main()
{
    ParticleProbe probe;


    auto data = probe.Inspect();


    assert(data.particleCount == 0);


    return 0;
}