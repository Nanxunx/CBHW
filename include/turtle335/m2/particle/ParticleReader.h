#pragma once

#include <cstdint>
#include <vector>


namespace turtle335
{


struct ParticleEmitterInfo
{
    std::uint32_t index = 0;

    std::uint32_t offset = 0;

    std::uint32_t size = 0;
};



class ParticleReader
{
public:

    std::vector<ParticleEmitterInfo> Read(
        std::uint32_t offset,
        std::uint32_t count
    );

};


}