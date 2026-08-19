#include "turtle335/m2/ClassicM2Writer.h"

#include "turtle335/m2/AnimationMetadata.h"
#include "turtle335/m2/AuxiliaryTrackWriters.h"
#include "turtle335/m2/BinaryBuilder.h"
#include "turtle335/m2/BoneWriter.h"
#include "turtle335/m2/CameraWriter.h"
#include "turtle335/m2/ClassicM2Header.h"
#include "turtle335/m2/ClassicM2Validator.h"
#include "turtle335/m2/EventWriter.h"
#include "turtle335/m2/LightWriter.h"
#include "turtle335/m2/ParticleWriter.h"
#include "turtle335/m2/RibbonWriter.h"
#include "turtle335/m2/SkinViewWriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace turtle335::m2 {
namespace {

std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, const std::size_t o, const char* what)
{
    if (o > d.size() || 4u > d.size() - o)
        throw std::runtime_error(std::string(what) + " OOB");
    return static_cast<std::uint32_t>(d[o]) |
           (static_cast<std::uint32_t>(d[o + 1u]) << 8u) |
           (static_cast<std::uint32_t>(d[o + 2u]) << 16u) |
           (static_cast<std::uint32_t>(d[o + 3u]) << 24u);
}

void PutU16(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint16_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
}

void PutU32(std::vector<std::uint8_t>& d, const std::size_t o, const std::uint32_t v)
{
    d[o] = static_cast<std::uint8_t>(v & 0xffu);
    d[o + 1u] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
    d[o + 2u] = static_cast<std::uint8_t>((v >> 16u) & 0xffu);
    d[o + 3u] = static_cast<std::uint8_t>((v >> 24u) & 0xffu);
}

std::uint32_t CurrentOffset(const BinaryBuilder& output)
{
    if (output.Bytes().size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        throw std::overflow_error("M2 output offset exceeds uint32");
    return static_cast<std::uint32_t>(output.Bytes().size());
}

void Align4(BinaryBuilder& output)
{
    const std::size_t pad = (4u - (output.Bytes().size() & 3u)) & 3u;
    if (pad != 0u)
    {
        static const std::array<std::uint8_t, 3> zeros{{0u, 0u, 0u}};
        output.Append(zeros.data(), pad);
    }
}

M2ArrayRef AppendRawArray(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef ref,
    const std::size_t stride,
    const char* what)
{
    M2ArrayRef out{ref.count, 0u};
    if (ref.count == 0u)
        return out;
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * stride;
    if (ref.offset == 0u || static_cast<std::size_t>(ref.offset) > source.size() ||
        bytes > source.size() - static_cast<std::size_t>(ref.offset))
        throw std::runtime_error(std::string(what) + " source array OOB");
    Align4(output);
    out.offset = output.Append(source.data() + ref.offset, bytes);
    return out;
}

M2ArrayRef AppendBytesAsArray(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& bytes,
    const std::uint32_t count)
{
    M2ArrayRef out{count, 0u};
    if (bytes.empty())
        return out;
    Align4(output);
    out.offset = output.Append(bytes);
    return out;
}

M2ArrayRef AppendInt16Array(
    BinaryBuilder& output,
    const std::vector<std::int16_t>& values)
{
    std::vector<std::uint8_t> raw(values.size() * 2u, 0u);
    for (std::size_t i = 0u; i < values.size(); ++i)
        PutU16(raw, i * 2u, static_cast<std::uint16_t>(values[i]));
    return AppendBytesAsArray(output, raw, static_cast<std::uint32_t>(values.size()));
}

M2ArrayRef AppendPlayable(
    BinaryBuilder& output,
    const std::vector<PlayableAnimationRecord>& values)
{
    std::vector<std::uint8_t> raw(values.size() * 4u, 0u);
    for (std::size_t i = 0u; i < values.size(); ++i)
    {
        PutU16(raw, i * 4u + 0u, static_cast<std::uint16_t>(values[i].fallbackAnimationId));
        PutU16(raw, i * 4u + 2u, static_cast<std::uint16_t>(values[i].flags));
    }
    return AppendBytesAsArray(output, raw, static_cast<std::uint32_t>(values.size()));
}

M2ArrayRef AppendCanonicalName(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceName)
{
    if (sourceName.count == 0u)
        return {};
    if (sourceName.offset == 0u || static_cast<std::size_t>(sourceName.offset) > source.size() ||
        static_cast<std::size_t>(sourceName.count) > source.size() - static_cast<std::size_t>(sourceName.offset))
        throw std::runtime_error("M2 source name OOB");
    std::vector<std::uint8_t> name(
        source.begin() + static_cast<std::ptrdiff_t>(sourceName.offset),
        source.begin() + static_cast<std::ptrdiff_t>(sourceName.offset + sourceName.count));
    // Successful Turtle/112 Golden targets consistently use source count + 1
    // and include the terminating NUL in the target name length.
    name.push_back(0u);
    Align4(output);
    return {static_cast<std::uint32_t>(name.size()), output.Append(name)};
}

M2ArrayRef AppendTextures(
    BinaryBuilder& output,
    const std::vector<std::uint8_t>& source,
    const M2ArrayRef sourceTextures)
{
    constexpr std::size_t stride = 16u;
    M2ArrayRef target{sourceTextures.count, 0u};
    if (sourceTextures.count == 0u)
        return target;
    const std::size_t sourceBytes = static_cast<std::size_t>(sourceTextures.count) * stride;
    if (sourceTextures.offset == 0u || static_cast<std::size_t>(sourceTextures.offset) > source.size() ||
        sourceBytes > source.size() - static_cast<std::size_t>(sourceTextures.offset))
        throw std::runtime_error("M2 texture definitions OOB");

    Align4(output);
    target.offset = output.Reserve(sourceBytes);
    for (std::uint32_t i = 0u; i < sourceTextures.count; ++i)
    {
        const std::size_t so = static_cast<std::size_t>(sourceTextures.offset) + static_cast<std::size_t>(i) * stride;
        const std::uint32_t to = target.offset + i * static_cast<std::uint32_t>(stride);
        std::array<std::uint8_t, stride> rec{};
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(so), 8u, rec.begin());
        const std::uint32_t nameCount = ReadU32(source, so + 8u, "texture name count");
        const std::uint32_t nameOffset = ReadU32(source, so + 12u, "texture name offset");
        PutU32(rec.data(), 8u, nameCount);
        if (nameCount != 0u)
        {
            if (nameOffset == 0u || static_cast<std::size_t>(nameOffset) > source.size() ||
                static_cast<std::size_t>(nameCount) > source.size() - static_cast<std::size_t>(nameOffset))
                throw std::runtime_error("texture name payload OOB");
            Align4(output);
            const std::uint32_t relocated = output.Append(source.data() + nameOffset, nameCount);
            PutU32(rec.data(), 12u, relocated);
        }
        output.Patch(to, rec.data(), rec.size());
    }
    return target;
}

std::string FormatValidationErrors(const ClassicM2ValidationResult& validation)
{
    std::ostringstream out;
    out << "generated Classic/Turtle M2 failed strict validation";
    for (const auto& issue : validation.issues)
        out << "\n  " << issue.code << ": " << issue.detail;
    return out.str();
}

} // namespace

ClassicM2WriteResult ConvertWotlkM2ToClassic(
    const std::vector<std::uint8_t>& sourceM2,
    const std::vector<std::vector<std::uint8_t>>& skinFiles,
    const std::vector<std::uint8_t>& animationDataDbc,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence,
    const ClassicM2WriteOptions& options)
{
    const auto source = ParseWotlkM2(sourceM2);
    if (source.viewCount != skinFiles.size())
        throw std::runtime_error("WotLK nViews does not match supplied .skin file count");
    if (source.lights.count != 0u && !options.allowReferenceGatedLights)
        throw std::runtime_error(
            "model contains Light records; LightWriter is reference-gated until a paired Golden is supplied");

    BinaryBuilder output(std::vector<std::uint8_t>(kClassicM2HeaderSize, 0u));
    ClassicM2HeaderFields header;
    header.globalFlags = CanonicalizeClassicGlobalFlags(source.globalFlags);
    header.boundsAndCollisionFloats = source.boundsAndCollisionFloats;

    const auto name = AppendCanonicalName(output, sourceM2, source.name);
    header.nameLength = name.count;
    header.nameOffset = name.offset;

    header.globalSequences = AppendRawArray(output, sourceM2, source.globalSequences, 4u, "globalSequences");

    const auto sequenceBytes = BuildClassicSequenceRecords(source.sequences);
    header.animations = AppendBytesAsArray(
        output, sequenceBytes, static_cast<std::uint32_t>(source.sequences.size()));

    header.animationLookup = AppendInt16Array(output, BuildClassicAnimationLookup(source.sequences));
    const auto graph = ParseBuild12340AnimationFallbackGraph(animationDataDbc);
    header.playableAnimationLookup = AppendPlayable(
        output, BuildClassicPlayableAnimationLookup(source.sequences, graph));

    const auto windows = BuildClassicSequenceWindows(source.sequences);
    header.bones = ConvertWotlkBones(
        output, sourceM2, source.bones, windows, source.sequences, externalBySequence).target;
    header.keyBoneLookup = AppendRawArray(output, sourceM2, source.keyBoneLookup, 2u, "keyBoneLookup");
    header.vertices = AppendRawArray(output, sourceM2, source.vertices, 48u, "vertices");

    if (!skinFiles.empty())
    {
        Align4(output);
        header.views.count = static_cast<std::uint32_t>(skinFiles.size());
        header.views.offset = CurrentOffset(output);
        const auto viewBytes = BuildClassicEmbeddedViews(skinFiles, header.views.offset);
        output.Append(viewBytes);
    }

    header.colors = ConvertWotlkColors(
        output, sourceM2, source.colors, windows, source.sequences, externalBySequence);
    header.textures = AppendTextures(output, sourceM2, source.textures);
    header.transparency = ConvertWotlkTransparency(
        output, sourceM2, source.transparency, windows, source.sequences, externalBySequence);
    header.unknownI = {};
    header.textureAnimations = ConvertWotlkTextureAnimations(
        output, sourceM2, source.textureAnimations, windows, source.sequences, externalBySequence);

    header.textureReplace = AppendRawArray(output, sourceM2, source.textureReplace, 2u, "textureReplace");
    header.renderFlags = AppendRawArray(output, sourceM2, source.renderFlags, 4u, "renderFlags");
    header.boneLookup = AppendRawArray(output, sourceM2, source.boneLookup, 2u, "boneLookup");
    header.textureLookup = AppendRawArray(output, sourceM2, source.textureLookup, 2u, "textureLookup");
    header.textureUnitLookup = AppendRawArray(output, sourceM2, source.textureUnitLookup, 2u, "textureUnitLookup");
    header.transparencyLookup = AppendRawArray(output, sourceM2, source.transparencyLookup, 2u, "transparencyLookup");
    header.textureAnimationLookup = AppendRawArray(output, sourceM2, source.textureAnimationLookup, 2u, "textureAnimationLookup");

    header.boundingTriangles = AppendRawArray(output, sourceM2, source.boundingTriangles, 2u, "boundingTriangles");
    header.boundingVertices = AppendRawArray(output, sourceM2, source.boundingVertices, 12u, "boundingVertices");
    header.boundingNormals = AppendRawArray(output, sourceM2, source.boundingNormals, 12u, "boundingNormals");

    header.attachments = ConvertWotlkAttachments(
        output, sourceM2, source.attachments, windows, source.sequences, externalBySequence);
    header.attachmentLookup = AppendRawArray(output, sourceM2, source.attachmentLookup, 2u, "attachmentLookup");
    header.events = ConvertWotlkEvents(
        output, sourceM2, source.events, windows, source.sequences, externalBySequence);
    if (source.lights.count != 0u)
        header.lights = ConvertWotlkLights(
            output, sourceM2, source.lights, windows, source.sequences, externalBySequence);
    header.cameras = ConvertWotlkCameras(
        output, sourceM2, source.cameras, windows, source.sequences, externalBySequence);
    header.cameraLookup = AppendRawArray(output, sourceM2, source.cameraLookup, 2u, "cameraLookup");

    const auto ribbons = ConvertWotlkRibbons(
        output, sourceM2, source.ribbons, windows, source.sequences, externalBySequence);
    header.ribbons = ribbons.target;
    header.particles = ConvertWotlkParticles(
        output, sourceM2, source.particles, windows, source.sequences, externalBySequence).target;

    const auto headerBytes = BuildClassicM2Header(header);
    output.Patch(0u, headerBytes.data(), headerBytes.size());

    ClassicM2WriteResult result;
    result.bytes = output.Bytes();
    result.droppedRibbonUnknown1NonZero = ribbons.droppedUnknown1NonZero;

    if (options.runStrictValidator)
    {
        const auto validation = ValidateClassicM2(result.bytes);
        if (!validation.valid)
            throw std::runtime_error(FormatValidationErrors(validation));
    }
    return result;
}

} // namespace turtle335::m2
