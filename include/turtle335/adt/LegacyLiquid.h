#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace turtle335::adt {

enum class LiquidCategory : std::uint8_t
{
    Water,
    Ocean,
    Magma,
    Slime,
    Unknown
};

enum class LegacyLiquidSlot : std::uint8_t
{
    River = 0,
    Ocean = 1,
    Magma = 2,
    Slime = 3
};

struct LiquidVertex
{
    float height = 0.0f;
    std::optional<std::uint8_t> depth;
    std::optional<std::uint16_t> u;
    std::optional<std::uint16_t> v;
};

// Normalized source instance. Vertices are local (width+1)*(height+1), row-major.
struct LiquidLayer
{
    LiquidCategory category = LiquidCategory::Unknown;
    std::uint8_t offsetX = 0;
    std::uint8_t offsetY = 0;
    std::uint8_t width = 0;
    std::uint8_t height = 0;

    std::vector<bool> visible; // width*height, row-major
    std::vector<LiquidVertex> vertices;

    // MCNK-absolute 8x8 bitmaps, bit=(y*8+x).
    // The second MH2O attribute is commonly called Deep/Fatigue by different tools.
    // It is encoded into legacy MCLQ tile bit 7; fishable is tile bit 6.
    std::uint64_t deepMask = 0;
    std::uint64_t fishableMask = 0;
};

struct LegacyMclqVertex
{
    std::uint32_t lightOrUv = 0;
    float height = 0.0f;
};

// One 804-byte legacy MCLQ record. A target MCNK may contain at most one record
// for each category slot (River, Ocean, Magma, Slime), in that fixed order.
struct LegacyMclq
{
    LiquidCategory category = LiquidCategory::Unknown;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    std::array<LegacyMclqVertex, 81> vertices{};
    std::array<std::uint8_t, 64> cellFlags{};
    std::array<std::uint8_t, 84> flowData{};
};

struct LegacyMclqBlock
{
    std::array<std::optional<LegacyMclq>, 4> records{};
    std::uint32_t mcnkLiquidFlags = 0;
};

enum class LiquidDiagnosticKind
{
    OverlappingCells,
    SharedVertexHeightConflict,
    SharedVertexPayloadConflict,
    MissingUvSynthesized
};

struct LiquidDiagnostic
{
    LiquidDiagnosticKind kind{};
    int x = -1;
    int y = -1;
    std::string message;
};

struct LiquidBuildResult
{
    LegacyMclqBlock block;
    bool lossless = true;
    std::vector<LiquidDiagnostic> diagnostics;
};

constexpr std::uint8_t LegacyCellCode(LiquidCategory category) noexcept
{
    switch (category)
    {
        case LiquidCategory::Ocean: return 0x01;
        case LiquidCategory::Slime: return 0x03;
        case LiquidCategory::Water: return 0x04;
        case LiquidCategory::Magma: return 0x06;
        default: return 0x0F;
    }
}

constexpr std::uint32_t McnkLiquidFlag(LiquidCategory category) noexcept
{
    switch (category)
    {
        case LiquidCategory::Water: return 0x04;
        case LiquidCategory::Ocean: return 0x08;
        case LiquidCategory::Magma: return 0x10;
        case LiquidCategory::Slime: return 0x20;
        default: return 0;
    }
}

constexpr std::optional<LegacyLiquidSlot> SlotForCategory(LiquidCategory category) noexcept
{
    switch (category)
    {
        case LiquidCategory::Water: return LegacyLiquidSlot::River;
        case LiquidCategory::Ocean: return LegacyLiquidSlot::Ocean;
        case LiquidCategory::Magma: return LegacyLiquidSlot::Magma;
        case LiquidCategory::Slime: return LegacyLiquidSlot::Slime;
        default: return std::nullopt;
    }
}

constexpr std::size_t SlotIndex(LegacyLiquidSlot slot) noexcept
{
    return static_cast<std::size_t>(slot);
}

std::size_t LegacyMclqRecordCount(const LegacyMclqBlock& block) noexcept;
LiquidBuildResult BuildLegacyMclqBlock(const std::vector<LiquidLayer>& layers, float heightEpsilon = 1.0e-4f);

} // namespace turtle335::adt
