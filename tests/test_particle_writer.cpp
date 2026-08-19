#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/ParticleWriter.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d,std::size_t o,std::uint16_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
}
void PutU32(std::vector<std::uint8_t>& d,std::size_t o,std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}
std::uint16_t GetU16(const std::vector<std::uint8_t>& d,std::size_t o)
{
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o+1])<<8u);
}
std::uint32_t GetU32(const std::vector<std::uint8_t>& d,std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o+1])<<8u) |
           (static_cast<std::uint32_t>(d[o+2])<<16u) |
           (static_cast<std::uint32_t>(d[o+3])<<24u);
}

} // namespace

int main()
{
    using namespace turtle335::m2;
    constexpr std::uint32_t particleOffset=64u;
    constexpr std::uint32_t headKeys=800u;
    constexpr std::uint32_t tailKeys=816u;
    std::vector<std::uint8_t> source(1024u,0u);

    PutU32(source,particleOffset+0u,0x11223344u);
    PutU32(source,particleOffset+4u,0x12345678u);
    source[particleOffset+40u]=5u;
    source[particleOffset+41u]=2u;
    source[particleOffset+44u]=9u;
    source[particleOffset+45u]=10u;

    // Head/tail FakeAnimBlock key arrays; timestamp arrays intentionally empty.
    PutU32(source,particleOffset+316u+8u,4u);
    PutU32(source,particleOffset+316u+12u,headKeys);
    PutU32(source,particleOffset+332u+8u,4u);
    PutU32(source,particleOffset+332u+12u,tailKeys);
    for (std::uint16_t i=0;i<4u;++i)
    {
        PutU16(source,headKeys+i*2u,static_cast<std::uint16_t>(10u+i));
        PutU16(source,tailKeys+i*2u,static_cast<std::uint16_t>(20u+i));
    }

    // unknown-reference count remains zero. enabled track is empty and must
    // become the Classic legacy Times=[0], Keys=[1] representation.
    BinaryBuilder output;
    const std::vector<ClassicSequenceWindow> windows{{3333u,3433u}};
    const auto result=ConvertWotlkParticles(
        output,
        source,
        M2ArrayRef{1u,particleOffset},
        windows);

    assert(result.target.count==1u);
    assert(result.target.offset==0u);
    assert(result.convertedEmitters==1u);

    const auto& d=output.Bytes();
    assert(d.size()>=504u);
    assert(GetU32(d,0u)==0x11223344u);
    assert(GetU32(d,4u)==0x00005678u);
    assert(GetU16(d,40u)==5u);
    assert(GetU16(d,42u)==2u);
    assert(d[44u]==9u && d[45u]==10u);

    const std::uint16_t expectedCells[10]={10u,11u,1u,12u,13u,1u,20u,21u,22u,23u};
    for (std::size_t i=0;i<10u;++i)
        assert(GetU16(d,360u+i*2u)==expectedCells[i]);

    assert(GetU32(d,468u)==0u && GetU32(d,472u)==0u);
    // enabled Classic track at +476: no ranges, one timestamp, one key.
    assert(GetU32(d,480u)==0u);
    assert(GetU32(d,488u)==1u);
    assert(GetU32(d,496u)==1u);
    const auto timeOffset=GetU32(d,492u);
    const auto keyOffset=GetU32(d,500u);
    assert(GetU32(d,timeOffset)==0u);
    assert(d[keyOffset]==1u);

    // Selected V4.6 Golden shows that an empty group inside a non-empty
    // per-sequence enabled track also means enabled=1. Preserve an explicit
    // disabled key in the second sequence while synthesizing ones for the
    // first empty sequence.
    std::vector<std::uint8_t> source2(1024u,0u);
    const std::size_t enabled=particleOffset+456u;
    PutU16(source2,enabled+0u,0u);
    PutU16(source2,enabled+2u,0xffffu);
    PutU32(source2,enabled+4u,2u); PutU32(source2,enabled+8u,600u);
    PutU32(source2,enabled+12u,2u); PutU32(source2,enabled+16u,616u);
    PutU32(source2,600u,0u); PutU32(source2,604u,0u);
    PutU32(source2,608u,1u); PutU32(source2,612u,700u);
    PutU32(source2,616u,0u); PutU32(source2,620u,0u);
    PutU32(source2,624u,1u); PutU32(source2,628u,704u);
    PutU32(source2,700u,5u);
    source2[704u]=0u;

    WotlkM2Sequence seq0{}; seq0.length=100u; seq0.flags=0x20u;
    WotlkM2Sequence seq1{}; seq1.animationId=1u; seq1.length=200u; seq1.flags=0x20u;
    const std::vector<WotlkM2Sequence> sequences{seq0,seq1};
    const std::vector<ClassicSequenceWindow> windows2{{3333u,3433u},{6766u,6966u}};
    const std::vector<const std::vector<std::uint8_t>*> sidecars{nullptr,nullptr};
    BinaryBuilder output2;
    const auto result2=ConvertWotlkParticles(
        output2,source2,M2ArrayRef{1u,particleOffset},windows2,sequences,sidecars);
    assert(result2.convertedEmitters==1u);
    const auto& d2=output2.Bytes();
    const std::size_t enabledClassic=476u;
    assert(GetU32(d2,enabledClassic+20u)==4u);
    const auto keys2=GetU32(d2,enabledClassic+24u);
    assert(d2[keys2+0u]==1u);
    assert(d2[keys2+1u]==1u);
    assert(d2[keys2+2u]==0u);
    assert(d2[keys2+3u]==0u);

    std::cout << "PASS particle writer\n";
    return 0;
}
