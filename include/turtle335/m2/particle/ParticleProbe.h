#pragma once

#include <cstdint>
#include <vector>

#include "turtle335/m2/WotlkM2Reader.h"


namespace turtle335
{


struct ParticleProbeRecord
{
    std::uint32_t index = 0;

    std::uint32_t offset = 0;

    std::uint32_t size = 0;
};



struct ParticleProbeReport
{
    std::uint32_t emitterCount = 0;

    std::vector<ParticleProbeRecord> emitters;
};



class ParticleProbe
{
public:

    ParticleProbeReport Inspect(
        const m2::WotlkM2Document& model
    );

};


}