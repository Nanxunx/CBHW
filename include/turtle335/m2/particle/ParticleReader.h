#pragma once

#include <cstdint>
#include <vector>

#include "turtle335/m2/particle/ParticleData.h"


namespace turtle335
{


class ParticleReader
{
public:

    ParticleSystem Read(
        const std::vector<std::uint8_t>& data,
        std::uint32_t offset,
        std::uint32_t count
    );


private:

    ParticleEmitter ReadEmitter(
        const std::vector<std::uint8_t>& data,
        std::uint32_t offset,
        std::uint32_t index
    );

};


}