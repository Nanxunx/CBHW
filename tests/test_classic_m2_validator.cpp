#include "turtle335/m2/ClassicM2Validator.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
void PutU32(std::vector<std::uint8_t>& d,std::size_t o,std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}

bool HasIssueDetail(const turtle335::m2::ClassicM2ValidationResult& result,const std::string& needle)
{
    for (const auto& issue:result.issues)
        if (issue.detail.find(needle)!=std::string::npos)
            return true;
    return false;
}

bool HasIssueCode(const turtle335::m2::ClassicM2ValidationResult& result,const std::string& code)
{
    for (const auto& issue:result.issues)
        if (issue.code==code)
            return true;
    return false;
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
    assert(HasIssueDetail(result,"particleEmitters"));

    bad=good;
    PutU32(bad,0x2cu,225u);
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    assert(HasIssueCode(result,"PLAYABLE_COUNT"));

    // Newly integrated whole-writer blocks must be covered by strict top-level
    // span validation, not only Ribbon/Particle.
    bad=good;
    PutU32(bad,0x54u,1u); // Color56
    PutU32(bad,0x58u,static_cast<std::uint32_t>(bad.size()-8u));
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    assert(HasIssueDetail(result,"colors"));

    bad=good;
    PutU32(bad,0x104u,1u); // Attachment48
    PutU32(bad,0x108u,static_cast<std::uint32_t>(bad.size()-8u));
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    assert(HasIssueDetail(result,"attachments"));

    bad=good;
    PutU32(bad,0x124u,1u); // Camera124
    PutU32(bad,0x128u,static_cast<std::uint32_t>(bad.size()-8u));
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    assert(HasIssueDetail(result,"cameras"));

    // A texture definition can be in-bounds while its relocated filename
    // payload is not. The strict validator must catch that nested reference.
    bad=good;
    const std::uint32_t textureOffset=static_cast<std::uint32_t>(bad.size());
    bad.resize(bad.size()+16u,0u);
    PutU32(bad,0x5cu,1u);
    PutU32(bad,0x60u,textureOffset);
    PutU32(bad,static_cast<std::size_t>(textureOffset)+8u,4u);
    PutU32(bad,static_cast<std::size_t>(textureOffset)+12u,static_cast<std::uint32_t>(bad.size()-2u));
    result=ValidateClassicM2(bad);
    assert(!result.valid);
    assert(HasIssueCode(result,"TEXTURE_NAME_OOB"));

    std::cout<<"PASS classic m2 validator\n";
    return 0;
}