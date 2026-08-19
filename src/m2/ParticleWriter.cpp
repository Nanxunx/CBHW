#include "turtle335/m2/ParticleWriter.h"
#include "turtle335/m2/LegacyTrack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace turtle335::m2 {
namespace {

constexpr std::size_t kSourceStride = 476u;
constexpr std::size_t kTargetStride = 504u;
constexpr std::array<std::size_t,10> kSourceFloatTracks{{52u,72u,92u,112u,132u,152u,176u,200u,220u,240u}};
constexpr std::array<std::size_t,10> kTargetFloatTracks{{52u,80u,108u,136u,164u,192u,220u,248u,276u,304u}};

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o>d.size() || 2u>d.size()-o) throw std::runtime_error("Particle u16 OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o+1])<<8u);
}

std::int16_t ReadI16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return static_cast<std::int16_t>(ReadU16(d,o));
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o>d.size() || 4u>d.size()-o) throw std::runtime_error("Particle u32 OOB");
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o+1])<<8u) |
           (static_cast<std::uint32_t>(d[o+2])<<16u) |
           (static_cast<std::uint32_t>(d[o+3])<<24u);
}

float ReadF32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    const auto bits=ReadU32(d,o);
    float value=0.0f;
    std::memcpy(&value,&bits,sizeof(value));
    return value;
}

void PutU16(std::uint8_t* d,const std::size_t o,const std::uint16_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
}

void PutU32(std::uint8_t* d,const std::size_t o,const std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu);
    d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu);
    d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}

void PutF32(std::uint8_t* d,const std::size_t o,const float v)
{
    std::uint32_t bits=0u;
    std::memcpy(&bits,&v,sizeof(bits));
    PutU32(d,o,bits);
}

std::pair<std::uint32_t,std::uint32_t> ReadPair(const std::vector<std::uint8_t>& d,const std::size_t o)
{
    return {ReadU32(d,o),ReadU32(d,o+4u)};
}

std::uint32_t CopyBytes(BinaryBuilder& output,const std::vector<std::uint8_t>& source,const std::uint32_t count,const std::uint32_t offset)
{
    if (!count) return 0u;
    if (!offset || static_cast<std::size_t>(offset)>source.size() || static_cast<std::size_t>(count)>source.size()-offset)
        throw std::runtime_error("Particle filename OOB");
    return output.Append(source.data()+offset,count);
}

std::vector<std::uint16_t> ReadFakeU16Keys(const std::vector<std::uint8_t>& source,const std::size_t record,const std::size_t fakeOffset)
{
    const auto keys=ReadPair(source,record+fakeOffset+8u);
    if (!keys.first) return {};
    const std::size_t bytes=static_cast<std::size_t>(keys.first)*2u;
    if (!keys.second || static_cast<std::size_t>(keys.second)>source.size() || bytes>source.size()-keys.second)
        throw std::runtime_error("Particle fake u16 keys OOB");
    std::vector<std::uint16_t> out;
    out.reserve(keys.first);
    for (std::uint32_t i=0;i<keys.first;++i) out.push_back(ReadU16(source,static_cast<std::size_t>(keys.second)+i*2u));
    return out;
}

std::vector<std::int16_t> ReadFakeI16Keys(const std::vector<std::uint8_t>& source,const std::size_t record,const std::size_t fakeOffset)
{
    const auto keys=ReadPair(source,record+fakeOffset+8u);
    if (!keys.first) return {};
    const std::size_t bytes=static_cast<std::size_t>(keys.first)*2u;
    if (!keys.second || static_cast<std::size_t>(keys.second)>source.size() || bytes>source.size()-keys.second)
        throw std::runtime_error("Particle fake i16 keys OOB");
    std::vector<std::int16_t> out;
    out.reserve(keys.first);
    for (std::uint32_t i=0;i<keys.first;++i) out.push_back(ReadI16(source,static_cast<std::size_t>(keys.second)+i*2u));
    return out;
}

std::vector<std::uint16_t> ReadFakeTimes(const std::vector<std::uint8_t>& source,const std::size_t record,const std::size_t fakeOffset)
{
    const auto times=ReadPair(source,record+fakeOffset);
    if (!times.first) return {};
    const std::size_t bytes=static_cast<std::size_t>(times.first)*2u;
    if (!times.second || static_cast<std::size_t>(times.second)>source.size() || bytes>source.size()-times.second)
        throw std::runtime_error("Particle fake times OOB");
    std::vector<std::uint16_t> out;
    out.reserve(times.first);
    for (std::uint32_t i=0;i<times.first;++i) out.push_back(ReadU16(source,static_cast<std::size_t>(times.second)+i*2u));
    return out;
}

std::vector<std::array<float,3>> ReadFakeVec3Keys(const std::vector<std::uint8_t>& source,const std::size_t record,const std::size_t fakeOffset)
{
    const auto keys=ReadPair(source,record+fakeOffset+8u);
    if (!keys.first) return {};
    const std::size_t bytes=static_cast<std::size_t>(keys.first)*12u;
    if (!keys.second || static_cast<std::size_t>(keys.second)>source.size() || bytes>source.size()-keys.second)
        throw std::runtime_error("Particle fake vec3 keys OOB");
    std::vector<std::array<float,3>> out;
    out.reserve(keys.first);
    for (std::uint32_t i=0;i<keys.first;++i)
    {
        const std::size_t at=static_cast<std::size_t>(keys.second)+i*12u;
        out.push_back({ReadF32(source,at),ReadF32(source,at+4u),ReadF32(source,at+8u)});
    }
    return out;
}

std::vector<std::array<float,2>> ReadFakeVec2Keys(const std::vector<std::uint8_t>& source,const std::size_t record,const std::size_t fakeOffset)
{
    const auto keys=ReadPair(source,record+fakeOffset+8u);
    if (!keys.first) return {};
    const std::size_t bytes=static_cast<std::size_t>(keys.first)*8u;
    if (!keys.second || static_cast<std::size_t>(keys.second)>source.size() || bytes>source.size()-keys.second)
        throw std::runtime_error("Particle fake vec2 keys OOB");
    std::vector<std::array<float,2>> out;
    out.reserve(keys.first);
    for (std::uint32_t i=0;i<keys.first;++i)
    {
        const std::size_t at=static_cast<std::size_t>(keys.second)+i*8u;
        out.push_back({ReadF32(source,at),ReadF32(source,at+4u)});
    }
    return out;
}

std::uint8_t ColorByte(const float v)
{
    if (!(v>0.0f)) return 0u;
    if (v>=255.0f) return 255u;
    return static_cast<std::uint8_t>(static_cast<int>(v));
}

std::vector<std::uint8_t> ZeroKeyForInterpolation(const std::uint16_t interpolation,const std::size_t baseSize)
{
    const std::size_t multiplier=(interpolation==2u || interpolation==3u)?3u:1u;
    return std::vector<std::uint8_t>(baseSize*multiplier,0u);
}

} // namespace

ParticleConversionResult ConvertWotlkParticles(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceParticles,
    const std::vector<ClassicSequenceWindow>& windows)
{
    ParticleConversionResult result;
    result.target.count=sourceParticles.count;
    if (!sourceParticles.count) return result;

    const std::size_t total=static_cast<std::size_t>(sourceParticles.count)*kSourceStride;
    if (!sourceParticles.offset || sourceParticles.offset>source.size() || total>source.size()-sourceParticles.offset)
        throw std::runtime_error("Particle source records OOB");

    result.target.offset=output.Reserve(static_cast<std::size_t>(sourceParticles.count)*kTargetStride);

    for (std::uint32_t i=0;i<sourceParticles.count;++i)
    {
        const std::size_t so=static_cast<std::size_t>(sourceParticles.offset)+static_cast<std::size_t>(i)*kSourceStride;
        const std::uint32_t to=result.target.offset+i*static_cast<std::uint32_t>(kTargetStride);
        std::array<std::uint8_t,kTargetStride> rec{};

        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so),4u,rec.begin());
        PutU32(rec.data(),4u,ReadU32(source,so+4u)&0xffffu);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+8u),16u,rec.begin()+8u);

        const auto modelName=ReadPair(source,so+24u);
        const auto particleName=ReadPair(source,so+32u);
        PutU32(rec.data(),24u,modelName.first);
        PutU32(rec.data(),28u,CopyBytes(output,source,modelName.first,modelName.second));
        PutU32(rec.data(),32u,particleName.first);
        PutU32(rec.data(),36u,CopyBytes(output,source,particleName.first,particleName.second));

        PutU16(rec.data(),40u,source[so+40u]);
        PutU16(rec.data(),42u,source[so+41u]);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+44u),8u,rec.begin()+44u);

        for (std::size_t t=0;t<kSourceFloatTracks.size();++t)
        {
            const auto interpolation=ReadU16(source,so+kSourceFloatTracks[t]);
            const std::size_t keySize=4u*((interpolation==2u || interpolation==3u)?3u:1u);
            const auto wt=ParseWotlkTrack(source,so+kSourceFloatTracks[t],keySize);
            const auto flat=FlattenLegacyValueTrack(wt,windows,ZeroKeyForInterpolation(interpolation,4u));
            const auto ct=SerializeClassicTrack(output,wt,flat);
            std::copy(ct.begin(),ct.end(),rec.begin()+static_cast<std::ptrdiff_t>(kTargetFloatTracks[t]));
        }

        const auto colorTimes=ReadFakeTimes(source,so,260u);
        const auto colors=ReadFakeVec3Keys(source,so,260u);
        const auto opacity=ReadFakeI16Keys(source,so,276u);
        const auto sizes=ReadFakeVec2Keys(source,so,292u);

        if (colorTimes.size()==3u && colors.size()==3u)
        {
            PutF32(rec.data(),332u,static_cast<float>(colorTimes[1])/32767.0f);
            for (std::size_t c=0;c<3u;++c)
            {
                const auto a=c<opacity.size() ? static_cast<std::uint8_t>((static_cast<std::uint16_t>(opacity[c])>>7u)&0xffu) : 0u;
                rec[336u+c*4u+0u]=ColorByte(colors[c][2]);
                rec[336u+c*4u+1u]=ColorByte(colors[c][1]);
                rec[336u+c*4u+2u]=ColorByte(colors[c][0]);
                rec[336u+c*4u+3u]=a;
            }
        }
        if (sizes.size()==3u)
        {
            PutF32(rec.data(),348u,sizes[0][0]);
            PutF32(rec.data(),352u,sizes[1][0]);
            PutF32(rec.data(),356u,sizes[2][0]);
        }

        auto head=ReadFakeU16Keys(source,so,316u);
        auto tail=ReadFakeU16Keys(source,so,332u);
        head.resize(std::max<std::size_t>(head.size(),4u),0u);
        tail.resize(std::max<std::size_t>(tail.size(),4u),0u);
        const std::array<std::uint16_t,10> cells{{head[0],head[1],1u,head[2],head[3],1u,tail[0],tail[1],tail[2],tail[3]}};
        for (std::size_t c=0;c<cells.size();++c) PutU16(rec.data(),360u+c*2u,cells[c]);

        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+348u),12u,rec.begin()+380u);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+360u),12u,rec.begin()+392u);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+372u),4u,rec.begin()+404u);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+384u),4u,rec.begin()+408u);
        // target +412..419 stays zero; WotLK unknown3 Vec2 is dropped.
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+396u),12u,rec.begin()+420u);

        for (std::size_t axis=0;axis<3u;++axis)
        {
            float v=ReadF32(source,so+408u+axis*4u);
            if (v==0.0f) v=0.0f; // normalize -0 to +0
            PutF32(rec.data(),432u+axis*4u,v);
        }
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+420u),8u,rec.begin()+444u);
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+432u),16u,rec.begin()+452u);

        const auto unknownRef=ReadPair(source,so+448u);
        if (unknownRef.first!=0u)
            throw std::runtime_error("Particle nUnknownReference != 0 is not Golden-covered");
        PutU32(rec.data(),468u,0u);
        PutU32(rec.data(),472u,0u);

        const auto enabledInterpolation=ReadU16(source,so+456u);
        const std::size_t enabledKeySize=(enabledInterpolation==2u || enabledInterpolation==3u)?3u:1u;
        const auto enabled=ParseWotlkTrack(source,so+456u,enabledKeySize);
        FlattenedLegacyTrack enabledFlat;
        if (enabled.timestamps.empty())
        {
            enabledFlat.timestamps.push_back(0u);
            enabledFlat.keys.push_back(std::vector<std::uint8_t>(enabledKeySize,1u));
        }
        else
        {
            enabledFlat=FlattenLegacyValueTrack(enabled,windows,ZeroKeyForInterpolation(enabledInterpolation,1u));
        }
        const auto enabledClassic=SerializeClassicTrack(output,enabled,enabledFlat);
        std::copy(enabledClassic.begin(),enabledClassic.end(),rec.begin()+476u);

        output.Patch(to,rec.data(),rec.size());
        ++result.convertedEmitters;
    }

    return result;
}

} // namespace turtle335::m2
