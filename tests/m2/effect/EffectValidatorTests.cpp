#include "turtle335/m2/effect/EffectValidator.h"

#include <cassert>
#include <iostream>


using namespace turtle335::m2::effect;


int main()
{
    EffectValidator validator;


    std::vector<uint8_t> empty;


    auto result = validator.Validate(empty);


    assert(result.passed == false);


    std::cout
        << "EffectValidator basic test passed\n";


    return 0;
}