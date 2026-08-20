#include "turtle335/m2/particle/ParticleProbe.h"

#include <cassert>


using namespace turtle335;


int main()
{
    m2::WotlkM2Document model;


    model.particles.count = 3;

    model.particles.offset = 1000;


    ParticleProbe probe;


    auto report =
        probe.Inspect(model);



    assert(report.emitterCount == 3);


    assert(report.emitters.size() == 3);



    assert(report.emitters[0].offset == 1000);

    assert(report.emitters[1].offset == 1476);

    assert(report.emitters[2].offset == 1952);



    return 0;
}