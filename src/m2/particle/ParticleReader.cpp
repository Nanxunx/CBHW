#include "turtle335/m2/particle/ParticleReader.h"


namespace turtle335
{


std::vector<ParticleEmitterInfo>
ParticleReader::Read(
    std::uint32_t offset,
    std::uint32_t count
)
{
    std::vector<ParticleEmitterInfo> result;


    constexpr std::uint32_t kParticleStride = 476;


    for(std::uint32_t i = 0;
        i < count;
        ++i)
    {
        ParticleEmitterInfo info;


        info.index = i;


        info.offset =
            offset + i * kParticleStride;


        info.size =
            kParticleStride;


        result.push_back(info);
    }


    return result;
}


}