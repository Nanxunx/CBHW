#include "turtle335/m2/ClassicM2Validator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace turtle335::m2 {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d,const std::size_t o)
{
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o+1])<<8u) |
           (static_cast<std::uint32_t>(d[o+2])<<16u) |
           (static_cast<std::uint32_t>(d[o+3])<<24u);
}

std::pair<std::uint32_t,std::uint32_t> Pair(const std::vector<std::uint8_t>& d,const std::size_t o)
{
    return {ReadU32(d,o),ReadU32(d,o+4u)};
}

void Add(ClassicM2ValidationResult& r,const std::string& code,const std::string& detail)
{
    r.issues.push_back({code,detail});
}

bool SpanOk(const std::vector<std::uint8_t>& d,const std::uint32_t count,const std::uint32_t offset,const std::size_t stride)
{
    if (count==0u) return offset==0u || static_cast<std::size_t>(offset)<=d.size();
    if (offset==0u || static_cast<std::size_t>(offset)>d.size()) return false;
    const std::size_t bytes=static_cast<std::size_t>(count)*stride;
    return bytes<=d.size()-static_cast<std::size_t>(offset);
}

void CheckArray(
    ClassicM2ValidationResult& r,
    const std::vector<std::uint8_t>& d,
    const std::size_t headerOffset,
    const std::size_t stride,
    const char* name)
{
    const auto [count,offset]=Pair(d,headerOffset);
    if (!SpanOk(d,count,offset,stride))
    {
        std::ostringstream ss;
        ss<<name<<" count="<<count<<" offset="<<offset<<" stride="<<stride<<" file="<<d.size();
        Add(r,"ARRAY_OOB",ss.str());
    }
}

void CheckViews(ClassicM2ValidationResult& r,const std::vector<std::uint8_t>& d)
{
    const auto [count,offset]=Pair(d,0x4cu);
    if (!SpanOk(d,count,offset,44u))
    {
        Add(r,"VIEW_TABLE_OOB","embedded Classic View table is outside file");
        return;
    }
    for (std::uint32_t i=0;i<count;++i)
    {
        const std::size_t v=static_cast<std::size_t>(offset)+static_cast<std::size_t>(i)*44u;
        struct Spec { std::size_t off; std::size_t stride; const char* name; };
        const Spec specs[]={{0u,2u,"indices"},{8u,2u,"triangles"},{16u,4u,"properties"},{24u,32u,"submeshes"},{32u,24u,"textureUnits"}};
        for (const auto& s:specs)
        {
            const auto [n,o]=Pair(d,v+s.off);
            if (!SpanOk(d,n,o,s.stride))
            {
                std::ostringstream ss;
                ss<<"view "<<i<<" "<<s.name<<" count="<<n<<" offset="<<o;
                Add(r,"VIEW_CHILD_OOB",ss.str());
            }
        }
    }
}

void CheckTextureNames(ClassicM2ValidationResult& r,const std::vector<std::uint8_t>& d)
{
    constexpr std::size_t stride=16u;
    const auto [count,offset]=Pair(d,0x5cu);
    if (!SpanOk(d,count,offset,stride))
        return;

    for (std::uint32_t i=0u;i<count;++i)
    {
        const std::size_t rec=static_cast<std::size_t>(offset)+static_cast<std::size_t>(i)*stride;
        const auto [nameCount,nameOffset]=Pair(d,rec+8u);
        if (!SpanOk(d,nameCount,nameOffset,1u))
        {
            std::ostringstream ss;
            ss<<"texture "<<i<<" name count="<<nameCount<<" offset="<<nameOffset;
            Add(r,"TEXTURE_NAME_OOB",ss.str());
        }
    }
}

} // namespace

ClassicM2ValidationResult ValidateClassicM2(const std::vector<std::uint8_t>& bytes)
{
    ClassicM2ValidationResult result;
    if (bytes.size()<324u)
    {
        Add(result,"HEADER_TRUNCATED","Classic/Turtle MD20 v256 header requires at least 324 bytes");
        return result;
    }
    if (std::memcmp(bytes.data(),"MD20",4u)!=0)
        Add(result,"BAD_MAGIC","expected MD20");
    if (ReadU32(bytes,4u)!=256u)
        Add(result,"BAD_VERSION","expected MD20 version 256");

    const auto name=Pair(bytes,0x08u);
    if (!SpanOk(bytes,name.first,name.second,1u))
        Add(result,"NAME_OOB","model name string is outside file");

    CheckArray(result,bytes,0x14u,4u,"globalSequences");
    CheckArray(result,bytes,0x1cu,68u,"animations");
    CheckArray(result,bytes,0x24u,2u,"animationLookup");

    const auto playable=Pair(bytes,0x2cu);
    if (playable.first!=226u)
        Add(result,"PLAYABLE_COUNT","PlayableAnimationLookup must contain exactly 226 records");
    if (!SpanOk(bytes,playable.first,playable.second,4u))
        Add(result,"PLAYABLE_OOB","PlayableAnimationLookup is outside file");

    CheckArray(result,bytes,0x34u,108u,"bones");
    CheckArray(result,bytes,0x3cu,2u,"keyBoneLookup");
    CheckArray(result,bytes,0x44u,48u,"vertices");
    CheckViews(result,bytes);
    CheckArray(result,bytes,0x54u,56u,"colors");
    CheckArray(result,bytes,0x5cu,16u,"textures");
    CheckTextureNames(result,bytes);
    CheckArray(result,bytes,0x64u,28u,"transparency");
    CheckArray(result,bytes,0x74u,84u,"textureAnimations");
    CheckArray(result,bytes,0x7cu,2u,"textureReplace");
    CheckArray(result,bytes,0x84u,4u,"renderFlags");
    CheckArray(result,bytes,0x8cu,2u,"boneLookup");
    CheckArray(result,bytes,0x94u,2u,"textureLookup");
    CheckArray(result,bytes,0x9cu,2u,"textureUnitLookup");
    CheckArray(result,bytes,0xa4u,2u,"transparencyLookup");
    CheckArray(result,bytes,0xacu,2u,"textureAnimationLookup");

    CheckArray(result,bytes,0xecu,2u,"boundingTriangles");
    CheckArray(result,bytes,0xf4u,12u,"boundingVertices");
    CheckArray(result,bytes,0xfcu,12u,"boundingNormals");
    CheckArray(result,bytes,0x104u,48u,"attachments");
    CheckArray(result,bytes,0x10cu,2u,"attachmentLookup");
    CheckArray(result,bytes,0x114u,44u,"events");
    CheckArray(result,bytes,0x11cu,212u,"lights");
    CheckArray(result,bytes,0x124u,124u,"cameras");
    CheckArray(result,bytes,0x12cu,2u,"cameraLookup");
    CheckArray(result,bytes,0x134u,220u,"ribbonEmitters");
    CheckArray(result,bytes,0x13cu,504u,"particleEmitters");

    result.valid=result.issues.empty();
    return result;
}

} // namespace turtle335::m2