#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/RibbonWriter.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t>& d, std::size_t o, std::uint16_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
}

void PutI16(std::vector<std::uint8_t>& d, std::size_t o, std::int16_t v)
{
    PutU16(d,o,static_cast<std::uint16_t>(v));
}

void PutU32(std::vector<std::uint8_t>& d, std::size_t o, std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}

std::uint16_t GetU16(const std::vector<std::uint8_t>& d, std::size_t o)
{
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o+1])<<8u);
}

std::uint32_t GetU32(const std::vector<std::uint8_t>& d, std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o+1])<<8u) |
           (static_cast<std::uint32_t>(d[o+2])<<16u) |
           (static_cast<std::uint32_t>(d[o+3])<<24u);
}

float GetF32(const std::vector<std::uint8_t>& d, std::size_t o)
{
    float v=0.0f;
    std::memcpy(&v,d.data()+o,sizeof(v));
    return v;
}

} // namespace

int main()
{
    using namespace turtle335::m2;

    constexpr std::uint32_t ribbonOffset=64u;
    constexpr std::uint32_t timeOuterOffset=400u;
    constexpr std::uint32_t keyOuterOffset=408u;

    std::vector<std::uint8_t> source(512u,0u);

    // Stable prefix and static body markers.
    PutU32(source,ribbonOffset+0u,0x11223344u);
    PutU32(source,ribbonOffset+4u,7u);
    for (std::size_t i=0;i<16u;++i)
        source[ribbonOffset+116u+i]=static_cast<std::uint8_t>(0x80u+i);

    // Color track: interpolation=1, globalSequence=-1, one outer group with
    // zero keys. Golden legacy behavior synthesizes start/end + white color.
    PutU16(source,ribbonOffset+36u,1u);
    PutI16(source,ribbonOffset+38u,-1);
    PutU32(source,ribbonOffset+40u,1u);
    PutU32(source,ribbonOffset+44u,timeOuterOffset);
    PutU32(source,ribbonOffset+48u,1u);
    PutU32(source,ribbonOffset+52u,keyOuterOffset);
    PutU32(source,timeOuterOffset+0u,0u);
    PutU32(source,timeOuterOffset+4u,0u);
    PutU32(source,keyOuterOffset+0u,0u);
    PutU32(source,keyOuterOffset+4u,0u);

    // Non-zero WotLK priority/padding tail is deliberately dropped.
    PutU32(source,ribbonOffset+172u,1u);

    BinaryBuilder output;
    const std::vector<ClassicSequenceWindow> windows{{3333u,3433u}};
    const auto result=ConvertWotlkRibbons(
        output,
        source,
        M2ArrayRef{1u,ribbonOffset},
        windows);

    assert(result.target.count==1u);
    assert(result.target.offset==0u);
    assert(result.droppedUnknown1NonZero==1u);

    const auto& bytes=output.Bytes();
    assert(bytes.size()>=220u);
    assert(GetU32(bytes,0u)==0x11223344u);
    assert(GetU32(bytes,4u)==7u);
    for (std::size_t i=0;i<16u;++i)
        assert(bytes[148u+i]==static_cast<std::uint8_t>(0x80u+i));

    // Classic color track at +36.
    assert(GetU16(bytes,36u)==1u);
    assert(GetU16(bytes,38u)==0xffffu);
    assert(GetU32(bytes,40u)==2u); // one sequence + trailing [0,0]
    assert(GetU32(bytes,48u)==2u); // start/end timestamps
    assert(GetU32(bytes,56u)==2u); // two synthesized color keys

    const auto timeOffset=GetU32(bytes,52u);
    assert(GetU32(bytes,timeOffset+0u)==3333u);
    assert(GetU32(bytes,timeOffset+4u)==3433u);

    const auto keyOffset=GetU32(bytes,60u);
    for (std::size_t key=0;key<2u;++key)
    {
        const std::size_t at=static_cast<std::size_t>(keyOffset)+key*12u;
        assert(std::fabs(GetF32(bytes,at+0u)-1.0f)<0.00001f);
        assert(std::fabs(GetF32(bytes,at+4u)-1.0f)<0.00001f);
        assert(std::fabs(GetF32(bytes,at+8u)-1.0f)<0.00001f);
    }

    std::cout << "PASS ribbon writer\n";
    return 0;
}
