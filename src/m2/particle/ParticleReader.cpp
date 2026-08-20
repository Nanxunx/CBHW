#include "turtle335/m2/particle/ParticleReader.h"

#include <stdexcept>


namespace turtle335
{


namespace
{


constexpr std::uint32_t kParticleStride = 476;


std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& data,
    std::uint32_t offset
)
{
    if(offset + 4 > data.size())
    {
        throw std::runtime_error(
            "Particle uint32 read overflow"
        );
    }


    return
        static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}


}



ParticleSystem ParticleReader::Read(
    const std::vector<std::uint8_t>& data,
    std::uint32_t offset,
    std::uint32_t count
)
{
    ParticleSystem system;


    system.count = count;


    for(std::uint32_t i = 0;
        i < count;
        ++i)
    {
        auto emitterOffset =
            offset + i * kParticleStride;


        system.emitters.push_back(
            ReadEmitter(
                data,
                emitterOffset,
                i
            )
        );
    }


    return system;
}



ParticleEmitter ParticleReader::ReadEmitter(
    const std::vector<std::uint8_t>& data,
    std::uint32_t offset,
    std::uint32_t index
)
{
    if(offset + kParticleStride > data.size())
    {
        throw std::runtime_error(
            "Particle emitter outside file"
        );
    }


    ParticleEmitter emitter;


    emitter.index = index;

    emitter.offset = offset;


    emitter.flags =
        ReadU32(
            data,
            offset
        );


    emitter.textureId =
        ReadU32(
            data,
            offset + 32
        );


    return emitter;
}


}