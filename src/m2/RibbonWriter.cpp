#include "turtle335/m2/RibbonWriter.h"
#include "turtle335/m2/ExternalAnim.h"
#include "turtle335/m2/LegacyTrack.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace turtle335::m2 {
namespace {
constexpr std::size_t kSourceStride = 176u;
constexpr std::size_t kTargetStride = 220u;

std::uint16_t ReadU16(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 2u > d.size() - o) throw std::runtime_error("Ribbon u16 OOB");
    return static_cast<std::uint16_t>(d[o]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(d[o + 1]) << 8u);
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    if (o > d.size() || 4u > d.size() - o) throw std::runtime_error("Ribbon u32 OOB");
    return static_cast<std::uint32_t>(d[o]) | (static_cast<std::uint32_t>(d[o+1])<<8u) |
           (static_cast<std::uint32_t>(d[o+2])<<16u) | (static_cast<std::uint32_t>(d[o+3])<<24u);
}

void PutU32(std::uint8_t* d, const std::size_t o, const std::uint32_t v)
{
    d[o]=static_cast<std::uint8_t>(v&0xffu); d[o+1]=static_cast<std::uint8_t>((v>>8u)&0xffu);
    d[o+2]=static_cast<std::uint8_t>((v>>16u)&0xffu); d[o+3]=static_cast<std::uint8_t>((v>>24u)&0xffu);
}

std::pair<std::uint32_t,std::uint32_t> ReadPair(const std::vector<std::uint8_t>& d, const std::size_t o)
{
    return {ReadU32(d,o),ReadU32(d,o+4u)};
}

std::uint32_t CopyU16Array(BinaryBuilder& b,const std::vector<std::uint8_t>& src,const std::uint32_t n,const std::uint32_t o)
{
    if (!n) return 0u;
    const std::size_t bytes=static_cast<std::size_t>(n)*2u;
    if (!o || static_cast<std::size_t>(o)>src.size() || bytes>src.size()-o) throw std::runtime_error("Ribbon array OOB");
    return b.Append(src.data()+o,bytes);
}

std::vector<std::uint8_t> BaseDefault(const std::size_t baseSize, const bool colorOnes)
{
    std::vector<std::uint8_t> out(baseSize,0u);
    if (colorOnes)
    {
        if (baseSize != 12u) throw std::runtime_error("Ribbon color base key size must be 12");
        const std::array<float,3> ones{{1.0f,1.0f,1.0f}};
        std::memcpy(out.data(),ones.data(),out.size());
    }
    return out;
}

std::vector<std::uint8_t> ExpandDefaultForInterpolation(
    const std::vector<std::uint8_t>& base,
    const std::uint16_t interpolation)
{
    const std::size_t multiplier=(interpolation==2u || interpolation==3u) ? 3u : 1u;
    std::vector<std::uint8_t> out;
    out.reserve(base.size()*multiplier);
    for (std::size_t i=0;i<multiplier;++i) out.insert(out.end(),base.begin(),base.end());
    return out;
}
}

RibbonConversionResult ConvertWotlkRibbons(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceRibbons,
    const std::vector<ClassicSequenceWindow>& windows,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence)
{
    RibbonConversionResult result;
    result.target.count=sourceRibbons.count;
    if (!sourceRibbons.count) return result;
    const std::size_t total=static_cast<std::size_t>(sourceRibbons.count)*kSourceStride;
    if (!sourceRibbons.offset || sourceRibbons.offset>source.size() || total>source.size()-sourceRibbons.offset)
        throw std::runtime_error("Ribbon source records OOB");
    result.target.offset=output.Reserve(static_cast<std::size_t>(sourceRibbons.count)*kTargetStride);

    struct Spec { std::size_t src; std::size_t dst; std::size_t baseKeySize; bool colorOnes; };
    const std::array<Spec,6> specs{{
        {36u,36u,12u,true},{56u,64u,2u,false},{76u,92u,4u,false},
        {96u,120u,4u,false},{132u,164u,2u,false},{152u,192u,1u,false}
    }};

    for (std::uint32_t i=0;i<sourceRibbons.count;++i)
    {
        const std::size_t so=static_cast<std::size_t>(sourceRibbons.offset)+static_cast<std::size_t>(i)*kSourceStride;
        const std::uint32_t to=result.target.offset+i*static_cast<std::uint32_t>(kTargetStride);
        std::array<std::uint8_t,kTargetStride> rec{};
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so),20u,rec.begin());
        const auto tex=ReadPair(source,so+20u); const auto blend=ReadPair(source,so+28u);
        PutU32(rec.data(),20u,tex.first); PutU32(rec.data(),24u,CopyU16Array(output,source,tex.first,tex.second));
        PutU32(rec.data(),28u,blend.first); PutU32(rec.data(),32u,CopyU16Array(output,source,blend.first,blend.second));
        std::copy_n(source.begin()+static_cast<std::ptrdiff_t>(so+116u),16u,rec.begin()+148u);

        for (const auto& s: specs)
        {
            const std::uint16_t interpolation=ReadU16(source,so+s.src);
            const std::size_t multiplier=(interpolation==2u || interpolation==3u) ? 3u : 1u;
            const std::size_t keySize=s.baseKeySize*multiplier;
            const auto wt = sequences.empty()
                ? ParseWotlkTrack(source,so+s.src,keySize)
                : ParseWotlkTrackWithExternal(
                    source,so+s.src,keySize,sequences,externalBySequence);
            const auto defaultKey=ExpandDefaultForInterpolation(BaseDefault(s.baseKeySize,s.colorOnes),interpolation);
            const auto flat=FlattenLegacyValueTrack(
                wt,windows,defaultKey,LegacySingleKeyPolicy::PerSequence);
            const auto ct=SerializeClassicTrack(output,wt,flat);
            std::copy(ct.begin(),ct.end(),rec.begin()+static_cast<std::ptrdiff_t>(s.dst));
        }
        if (ReadU32(source,so+172u)!=0u) ++result.droppedUnknown1NonZero;
        output.Patch(to,rec.data(),rec.size());
    }
    return result;
}

} // namespace turtle335::m2
