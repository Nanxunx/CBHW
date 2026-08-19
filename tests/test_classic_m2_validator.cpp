#include "turtle335/m2/ClassicM2Validator.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
void PutU32(std::vector<std::uint8_t>& d,std::size_t o,std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}
}

int main()
{
    using namespace turtle335::m2;

    // Minimal structurally valid target: header + mandatory 226-entry
    // PlayableAnimationLookup. All optional model arrays are empty.
    std::vector<std::uint8_t> good(324u+226u*4u,0u);
    std::memcpy(good.data(),"MD20",4u);
    PutU32(good,4u,256u);
    PutU32(good,0x2cu,226u);
    PutU32(good,0x30u,324u);
    auto result=ValidateClassicM2(good);
    assert(result.valid);
    assert(result.issues.empty());

    auto bad=good;
    PutU32(bad,0x13cu,1u);
    PutU32(bad,0x140u,static_cast<std::uint32_t>(bad.size()-100u));
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    bool foundParticle=false;
    for (const auto& issue:result.issues)
        if (issue.detail.find("particleEmitters")!=std::string::npos)
            foundParticle=true;
    assert(foundParticle);

    bad=good;
    PutU32(bad,0x2cu,225u);
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    bool foundPlayable=false;
    for (const auto& issue:result.issues)
        if (issue.code=="PLAYABLE_COUNT") foundPlayable=true;
    assert(foundPlayable);

    std::cout<<"PASS classic m2 validator\n";
    return 0;
}
