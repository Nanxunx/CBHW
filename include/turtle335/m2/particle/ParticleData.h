#pragma once

#include <cstdint>
#include <vector>


namespace turtle335
{


struct ParticleEmitter
{
    std::uint32_t index = 0;

    std::uint32_t offset = 0;

    std::uint32_t flags = 0;

    std::uint32_t textureId = 0;


    float emissionRate = 0.0f;

    float lifespan = 0.0f;

    float speed = 0.0f;

    float gravity = 0.0f;
};



struct ParticleSystem
{
    std::uint32_t count = 0;

    std::vector<ParticleEmitter> emitters;
};


}