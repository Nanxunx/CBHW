#pragma once

#include <vector>


namespace turtle335
{


struct ParticleProbeData
{
    int particleCount = 0;

    int textureIndex = -1;

    float alpha = 1.0f;

    int blendMode = 0;

    int animationId = -1;
};



class ParticleProbe
{
public:

    ParticleProbeData Inspect();


};


}