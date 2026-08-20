#include "turtle335/m2/particle/ParticleProbe.h"


namespace turtle335
{


ParticleProbeReport ParticleProbe::Inspect(
    const m2::WotlkM2Document& model
)
{
    ParticleProbeReport report;


    report.emitterCount =
        model.particles.count;


    constexpr std::uint32_t kParticleStride = 476;


    for(std::uint32_t i = 0;
        i < model.particles.count;
        ++i)
    {
        ParticleProbeRecord record;


        record.index = i;


        record.size =
            kParticleStride;


        if(model.particles.offset)
        {
            record.offset =
                model.particles.offset +
                i * kParticleStride;
        }


        report.emitters.push_back(record);
    }


    return report;
}


}