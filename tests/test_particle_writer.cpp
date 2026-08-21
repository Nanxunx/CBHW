#include "turtle335/m2/ParticleWriter.h"
#include "turtle335/m2/BinaryBuilder.h"

#include <cassert>
#include <cstdint>
#include <vector>
#include <iostream>


using namespace turtle335::m2;


int main()
{
    try
    {
        constexpr uint32_t emitterOffset = 64;


        std::vector<uint8_t> source(
            emitterOffset + 476,
            0
        );


        M2ArrayRef particles{};

        particles.count = 1;

        particles.offset = emitterOffset;



        BinaryBuilder output;


        std::vector<ClassicSequenceWindow> windows;



        auto result =
            ConvertWotlkParticles(
                output,
                source,
                particles,
                windows
            );


        std::cout
            << "convertedEmitters="
            << result.convertedEmitters
            << "\n";


        std::cout
            << "targetCount="
            << result.target.count
            << "\n";


        assert(result.convertedEmitters == 1);

        assert(result.target.count == 1);



        std::cout
            << "ParticleWriter test PASS\n";


        return 0;

    }
    catch(const std::exception& e)
    {
        std::cerr
            << "EXCEPTION: "
            << e.what()
            << "\n";

        return 1;
    }
}
