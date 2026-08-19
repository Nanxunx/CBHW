#pragma once

#include "turtle335/m2/LegacyTrack.h"
#include "turtle335/m2/WotlkM2Reader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace turtle335::m2 {

// WotLK sequence flag 0x20 means the animation payload is stored in the main
// M2. When it is clear, non-empty per-sequence track payloads live in the
// matching <Model><AnimID:04>-<SubID:02>.anim sidecar.
bool SequenceUsesExternalAnimSidecar(const WotlkM2Sequence& sequence) noexcept;

std::string BuildWotlkAnimSidecarFilename(
    std::string_view modelStem,
    const WotlkM2Sequence& sequence);

// Parse a 20-byte WotLK nested animation track while resolving per-sequence
// inner payloads from .anim sidecars when required by sequence flags.
//
// `keySize` is the physical size of one source key. For spline interpolation
// callers must pass the full source key size (base size * 3), matching the
// existing ParseWotlkTrack contract.
//
// `externalBySequence[i]` is nullptr for an unavailable/not-needed sidecar or
// points to the raw bytes of the .anim belonging to sequences[i]. Outer
// ArrayRefs and inner count/offset pairs are always read from the main M2;
// only the payload addressed by an inner offset switches to the sidecar.
WotlkTrackData ParseWotlkTrackWithExternal(
    const std::vector<std::uint8_t>& mainM2,
    std::size_t trackOffset,
    std::size_t keySize,
    const std::vector<WotlkM2Sequence>& sequences,
    const std::vector<const std::vector<std::uint8_t>*>& externalBySequence);

} // namespace turtle335::m2
