#include "turtle335/m2/particle/ParticleReader.h"

#include <cassert>
#include <vector>


using namespace turtle335;


int main()
{
    ParticleReader reader;


    /*
        创建3个ParticleEmitter假数据

        每个Emitter:
        476 bytes

        总大小:
        1428 bytes
    */

    std::vector<std::uint8_t> data(
        476 * 3,
        0
    );


    auto particles =
        reader.Read(
            data,
            0,
            3
        );


    assert(
        particles.count == 3
    );


    assert(
        particles.emitters.size() == 3
    );


    assert(
        particles.emitters[0].offset == 0
    );


    assert(
        particles.emitters[1].offset == 476
    );


    assert(
        particles.emitters[2].offset == 952
    );


    assert(
        particles.emitters[0].index == 0
    );


    assert(
        particles.emitters[1].index == 1
    );


    assert(
        particles.emitters[2].index == 2
    );


    return 0;
}