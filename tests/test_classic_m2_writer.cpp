#include "turtle335/m2/ClassicM2Validator.h"
#include "turtle335/m2/ClassicM2Writer.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d,const std::size_t o,const std::uint16_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1u]=static_cast<std::uint8_t>((v>>8u)&0xffu);
}

void PutU32(std::vector<std::uint8_t>& d,const std::size_t o,const std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1u]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2u]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3u]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}

std::uint32_t GetU32(const std::vector<std::uint8_t>& d,const std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o+1u])<<8u) |
           (static_cast<std::uint32_t>(d[o+2u])<<16u) |
           (static_cast<std::uint32_t>(d[o+3u])<<24u);
}

std::vector<std::uint8_t> MakeSource()
{
    constexpr std::uint32_t nameOff=304u;
    constexpr std::uint32_t seqOff=308u;
    std::vector<std::uint8_t> d(seqOff+64u,0u);
    std::memcpy(d.data(),"MD20",4u);
    PutU32(d,4u,264u);
    PutU32(d,8u,4u); PutU32(d,12u,nameOff);
    PutU32(d,16u,0x8u); // WotLK combiner-extension flag: target must clear.
    PutU32(d,0x1cu,1u); PutU32(d,0x20u,seqOff);
    std::memcpy(d.data()+nameOff,"Test",4u);
    PutU16(d,seqOff+0u,0u);
    PutU16(d,seqOff+2u,0u);
    PutU32(d,seqOff+4u,100u);
    PutU32(d,seqOff+12u,0x20u);
    PutU16(d,seqOff+62u,0u);
    return d;
}

std::vector<std::uint8_t> MakeAnimationDataDbc()
{
    constexpr std::uint32_t records=226u;
    constexpr std::uint32_t stride=32u;
    std::vector<std::uint8_t> d(20u+records*stride+1u,0u);
    std::memcpy(d.data(),"WDBC",4u);
    PutU32(d,4u,records); PutU32(d,8u,8u); PutU32(d,12u,stride); PutU32(d,16u,1u);
    for (std::uint32_t i=0u;i<records;++i)
    {
        const std::size_t row=20u+static_cast<std::size_t>(i)*stride;
        PutU32(d,row,i);
        PutU32(d,row+20u,0u);
    }
    return d;
}

} // namespace

int main()
{
    using namespace turtle335::m2;
    const auto result=ConvertWotlkM2ToClassic(MakeSource(),{},MakeAnimationDataDbc());
    const auto& d=result.bytes;
    assert(d.size()>324u);
    assert(std::memcmp(d.data(),"MD20",4u)==0);
    assert(GetU32(d,4u)==256u);
    assert(GetU32(d,8u)==5u);
    const auto nameOff=GetU32(d,12u);
    assert(std::memcmp(d.data()+nameOff,"Test\0",5u)==0);
    assert(GetU32(d,16u)==0u);
    assert(GetU32(d,0x1cu)==1u);
    assert(GetU32(d,0x2cu)==226u);
    const auto validation=ValidateClassicM2(d);
    assert(validation.valid);
    std::cout<<"PASS canonical whole-M2 baseline writer\n";
    return 0;
}
