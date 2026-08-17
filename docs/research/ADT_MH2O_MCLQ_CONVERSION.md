# ADT MH2O -> MCLQ Conversion Specification

状态：**第一版转换算法冻结；核心结构由 3.3.5 source + Turtle real-client binary 交叉确认**

目标：把 WoW 3.3.5a build 12340 的 `MH2O` 多层液体转换成 Turtle WoW 1.18.1 / Vanilla-derived 客户端真实支持的 legacy `MCLQ`。

---

## 1. 必要性

Turtle WoW 1.18.1 真客户端 terrain loader 已确认走 Vanilla legacy 路径：

```text
0x6C2010  ADT/MHDR root parse
0x6AF970  MCNK legacy subchunk fixup
0x68D540  legacy MCLQ surface build
```

未发现 build12340-style MH2O 输入路径。

因此客户端资产必须：

```text
3.3.5a MH2O -> legacy MCLQ
```

服务器 `.map` 生成仍建议直接解析源 MH2O，不先降级。

---

# 2. Source MH2O Reader

3.3.5 source Reader 以 TrinityCore/AzerothCore build12340 定义为主要依据。

每个 MCNK 对应 MH2O header：

```cpp
struct Mh2oHeaderEntry
{
    uint32_t offsetInstances;
    uint32_t layerCount;
    uint32_t offsetAttributes;
};
```

每个 instance：

```cpp
struct Mh2oInstance335
{
    uint16_t liquidType;
    uint16_t vertexFormat;

    float minHeightLevel;
    float maxHeightLevel;

    uint8_t offsetX;
    uint8_t offsetY;
    uint8_t width;
    uint8_t height;

    uint32_t offsetExistsBitmap;
    uint32_t offsetVertexData;
};
```

MH2O offsets are relative to MH2O **payload start**, not to the file or chunk-header start.

---

## 3. Attributes

Per-MCNK optional attributes：

```cpp
struct Mh2oAttributes
{
    uint64_t fishable;
    uint64_t deep;
};
```

These are 8x8 tile bitmaps for the MCNK.

Normalized representation retains both even if target legacy MCLQ cannot encode all source gameplay metadata.

---

# 4. Source liquid vertex formats

Primary TC/AZ build12340 reader recognizes：

```text
LVF 0 = Height + Depth
LVF 1 = Height + TextureCoord
LVF 2 = Depth only
```

A robust parser may also recognize LVF3 (`Height + UV + Depth`) when encountered, but it must be diagnostic-labelled because the primary TC 3.3.5 extractor code path historically models 0..2.

### LVF0

Per vertex：

```text
float height
uint8 depth
```

Conceptually 5 bytes/vertex, stored as height array followed by depth array in the TC reader's layout interpretation.

### LVF1

Per vertex semantics：

```text
float height
uint16 u
uint16 v
```

### LVF2

```text
uint8 depth
```

No explicit height array. Use `minHeightLevel` as the canonical flat target height when no vertex heights exist.

### LVF3 (robust optional)

```text
height + UV + depth
```

If accepted by the source parser：

- target water/ocean uses height + depth
- target magma/slime uses height + UV

---

# 5. Target MCLQ physical layout

Tortoise and old Trinity/MaNGOS structures agree：

```cpp
struct MclqVertexLegacy
{
    uint32_t lightOrUv;
    float height;
};

struct MclqPayloadLegacy
{
    float minHeight;
    float maxHeight;

    MclqVertexLegacy vertices[9][9]; // 81 * 8 = 648
    uint8_t flags[8][8];             // 64

    uint8_t flowData[84];
};
```

Payload：

```text
8 + 648 + 64 + 84 = 804 bytes
```

Full chunk including FourCC+size：

```text
812 bytes
```

---

## 6. Turtle real-client MCLQ pointer fixup

Function around：

```text
0x6AF760
```

real client splits MCLQ into：

```text
height1       first float
height2       second float
vertex ptr    payload + 8
flags ptr     payload + 8 + 0x288
flow count    first dword after flags
flow array    remaining 80 bytes
```

This confirms the 84-byte tail is not generic padding.

For canonical retroport output with no reconstructed legacy flow vectors：

```text
flow count = 0
remaining 80 bytes = 0
```

This deliberately disables optional legacy directional-flow effects while preserving the liquid surface itself.

Classification：**safe conservative target policy; visual flow effects may be lost and should be reported if a future source mapping is added.**

---

# 7. Target cell visibility/type byte — real-client behavior

Turtle liquid index builder：

```text
0x68D9B0
```

reads every 8x8 cell：

```asm
flag = MCLQ.flags[y][x]
kind = flag & 0x0F
```

Confirmed target values used by real client render paths：

```text
0x0F = hidden / no liquid cell
0x01 = ocean family
0x04 = river/water family
0x06 = magma/slime render family
```

High bits do not participate in this low-nibble type comparison.

The mapping is consistent with old MCLQ format comments/source and MCNK liquid-category flags.

---

## 8. Dark-water bit

Legacy Tortoise/TC extractor confirms：

```text
flag & 0x80 -> dark/deep water
```

Target generation：

```cpp
if (category == Ocean && sourceDeepBitForCell)
    targetCellFlag |= 0x80;
```

Prefer per-cell mapping from the source `Mh2oAttributes.deep` bitmap rather than promoting one deep bit to the whole MCNK.

This preserves more source information than the old server extractor, which only needed a chunk-level dark-water flag.

---

## 9. Fishable attribute

`MH2O.fishable` is retained in `NormalizedLiquidLayer` metadata.

Current Turtle MCLQ render/index paths inspected do not provide a proven equivalent bit that should be generated from MH2O fishability.

Therefore first production Writer：

```text
DO NOT invent an MCLQ fishable bit.
```

Record fishable metadata for diagnostics/server use and emit：

```text
info: source MH2O fishable bitmap is not represented in target MCLQ render encoding
```

This avoids confusing WMO MLIQ tile bitfield documentation with ADT MCLQ's low-nibble liquid selector.

---

# 10. MCNK category flags

Target MCNK category bits：

```text
0x04 River/Water
0x08 Ocean
0x10 Magma
0x20 Slime
```

For every target MCNK：

1. clear these four source/target category bits in the target header;
2. examine all successfully merged visible liquid cells;
3. set each category bit actually present.

Example：

```text
cells contain Water + Ocean
 -> MCNK flags |= 0x04 | 0x08
```

Magma and Slime both use legacy cell selector `0x06`, but retain distinct MCNK category bits where the target chunk has only one of those families.

If Magma and Slime both occur in the same MCNK and cannot be distinguished after cell-level encoding, classify as a legacy representability conflict and apply an explicit policy instead of silently claiming lossless conversion.

---

# 11. Exists bitmap -> target cell visibility

MH2O exists bitmap is instance-local and row-major in the TC extraction logic.

For each instance cell `(lx,ly)`：

```cpp
x = instance.offsetX + lx;
y = instance.offsetY + ly;
```

If source exists bit is clear：

```text
do not claim the target cell
```

If set：

```text
claim target MCLQ.flags[y][x]
```

Unclaimed target cells remain：

```text
0x0F
```

---

# 12. Vertex heights

Target MCLQ always contains one 9x9 height grid.

### Explicit-height formats

For LVF0/LVF1/(optional LVF3)：

```text
copy source instance vertex heights into target positions
[xOffset .. xOffset+width]
[yOffset .. yOffset+height]
```

### No-height format

For LVF2 or missing vertex data：

```text
height = instance.minHeightLevel
```

for all vertices required by the visible instance rectangle.

Source metadata explicitly defines `minHeightLevel` as the default level when no vertex heightmap exists.

---

## 13. Target min/max height

After merging the final representable target liquid surface：

```cpp
minHeight = min(height of vertices used by visible target cells);
maxHeight = max(height of vertices used by visible target cells);
```

If no visible cells exist, do not emit MCLQ.

All height inputs must be finite.

Do not simply copy one source instance's min/max when multiple instances were merged.

---

# 14. `lightOrUv` — two real legacy interpretations

The 4-byte field preceding every target vertex height is a union-like legacy field.

## Water / Ocean

Turtle functions around：

```text
0x68D690
0x68D790
```

consume the **lowest byte** of `lightOrUv` as an index into water/ocean shading/depth lookup tables.

Thus canonical source mapping：

```text
MH2O depth byte -> MCLQ lightOrUv low byte
```

Remaining high bytes can be zero in canonical output.

For source format without depth：

```text
depth = 0
```

unless a later fixture-based approximation is introduced.

## Magma / Slime

Turtle function：

```text
0x68D890
```

reads the same 4 bytes as：

```text
uint16 u
uint16 v
```

and multiplies each by real-client constant：

```text
0.01171875 = 3 / 256
```

Therefore：

```text
MH2O HeightTextureCoord u/v
 -> legacy MCLQ uint16 u/v
```

can be preserved directly where source UV data exists.

For magma/slime without UV data, generate deterministic legacy grid UVs and mark the layer as synthesized; do not leave every UV at zero because that collapses the texture sampling coordinates.

The exact synthesized grid formula must be fixture-validated before being classified as lossless.

---

# 15. Normalized vertex representation

```cpp
struct NormalizedLiquidVertex
{
    float height;

    std::optional<uint8_t> depth;
    std::optional<uint16_t> u;
    std::optional<uint16_t> v;
};
```

Do not store raw MCLQ `uint32 light` in normalized data.

The target Writer chooses the correct union interpretation from the liquid category.

---

# 16. Multiple MH2O instances — do NOT just take layer 0

MH2O can contain multiple instances for one MCNK.

Legacy MCLQ has：

```text
one 9x9 vertex grid
one 8x8 type/visibility grid
```

but that does **not** mean it can only represent one source instance.

Multiple source instances can be merged when their legacy representation is compatible.

---

## 17. Lossless merge case A — non-overlapping cells

If two instances claim different visible cells：

```text
Instance A cells ∩ Instance B cells = empty
```

they can often coexist in one MCLQ.

Example：

```text
left half  = water   selector 0x04
right half = ocean   selector 0x01
```

Target MCNK sets：

```text
0x04 | 0x08
```

and each cell keeps its own low-nibble selector.

---

## 18. Shared-vertex conflict detection

Even when visible cells do not overlap, adjacent liquid cells share 9x9 target vertices.

When assigning a target vertex already assigned by another source instance：

```cpp
if (abs(existingHeight - newHeight) > epsilon)
    conflict = SharedVertexHeightConflict;
```

This matters for two liquid surfaces touching at different elevations.

If separated by at least one hidden target cell, their independent heights can usually coexist without shared-corner conflict.

---

## 19. Overlapping-cell conflict

If two source instances both render the same target 8x8 cell：

```text
visible A ∩ visible B != empty
```

legacy MCLQ cannot represent both simultaneously because every cell has one type byte and one underlying height grid.

Classify：

```text
OverlappingLiquidLayers
```

Then use explicit fallback policy：

1. preserve primary/top gameplay-visible layer;
2. drop secondary layer from client target;
3. retain full source layers in diagnostics/server generation;
4. mark conversion `lossy`.

Never silently select `instances[0]`.

---

## 20. Magma/slime ambiguity

MCLQ's real-client render selector uses `0x06` for the magma/slime rendering family, while MCNK retains separate `0x10` Magma / `0x20` Slime flags.

If a single target MCNK requires both Magma and Slime in different cells, target representation may not preserve the semantic distinction cleanly.

Treat as：

```text
MagmaSlimeMixedLegacyConflict
```

until real-client fixtures establish a safe mixed encoding.

---

# 21. Target cell-flag builder

```cpp
uint8_t BaseMclqCellCode(LiquidCategory category)
{
    switch (category)
    {
        case LiquidCategory::Ocean:
            return 0x01;

        case LiquidCategory::Water:
            return 0x04;

        case LiquidCategory::Magma:
        case LiquidCategory::Slime:
            return 0x06;

        default:
            return 0x0F;
    }
}
```

Then：

```cpp
flag = BaseMclqCellCode(category);

if (category == Ocean && deep[y][x])
    flag |= 0x80;
```

Unclaimed/hidden：

```text
0x0F
```

No other high bit is generated by first-pass canonical Writer without binary-backed semantics.

---

# 22. Canonical target flow data

Tail：

```text
84 bytes
```

first-pass output：

```text
bytes 0..3   = uint32 0 flow count
bytes 4..83  = zero
```

Rationale：

- client explicitly handles flow count separately;
- count 0 represents no legacy flow vectors;
- MH2O does not carry an obvious one-to-one legacy flow-vector representation;
- zeroing is safer than copying unrelated WotLK bytes.

Diagnostic：

```text
legacy flow animation not reconstructed
```

is informational, not a geometry failure.

---

# 23. LiquidType.dbc mapper

Do not map `liquidType` ID numerically to Vanilla category.

Use source 3.3.5 `LiquidType.dbc` semantics, matching TC/AZ extraction：

```text
LiquidType ID
 -> LiquidType record
 -> SoundBank/category
 -> Water / Ocean / Magma / Slime
```

Normalized category is independent of the original DBC ID.

This is why the DBC conversion/research subsystem is a dependency of the ADT converter.

Unknown source type：

```text
ERROR or explicit configured fallback
```

not silent Water.

---

# 24. MCNK target flags update

Before liquid merge：

```cpp
targetFlags &= ~(0x04u | 0x08u | 0x10u | 0x20u);
```

After merge, set categories actually present.

If no visible target cells：

```text
no MCLQ emitted
offsMCLQ = 0
sizeMCLQ = 0/target-compatible empty convention
all four liquid category bits clear
```

Writer/validator must match the exact target MCNK convention used by known-good Vanilla/Turtle ADTs.

---

# 25. Target offset rebuild

For MCLQ-present MCNK：

```text
write MCLQ FourCC
write payload size = 804
write 804-byte payload
```

Then rebuild：

```text
MCNK.offsMCLQ
MCNK.sizeMCLQ
MCNK total chunk size
MCIN offset/size
MHDR / parent layout where affected
```

Never preserve WotLK source MCLQ offsets (usually absent/obsolete when MH2O is used).

---

# 26. Normalized merge structure

```cpp
struct TargetLegacyLiquidGrid
{
    std::array<uint8_t, 64> cellFlags;

    std::array<float, 81> heights;
    std::array<bool, 81> heightAssigned;

    std::array<uint8_t, 81> depth;
    std::array<uint16_t, 81> u;
    std::array<uint16_t, 81> v;

    std::array<bool, 81> depthAssigned;
    std::array<bool, 81> uvAssigned;

    uint32_t categoryMask;

    bool lossy;
    std::vector<LiquidConversionDiagnostic> diagnostics;
};
```

Initialize：

```text
all cells = 0x0F
all assigned flags = false
```

---

# 27. Merge algorithm

For each source MH2O instance：

```text
1. resolve LiquidType -> normalized category
2. parse LVF to normalized vertices
3. expand source exists bitmap
4. project visible source cells into target 8x8
5. detect cell overlap
6. assign/check shared vertex heights
7. assign target depth or UV semantics
8. map deep bitmap per cell
9. set MCNK target category bit
```

After all instances：

```text
if conflict-free:
    emit one legacy MCLQ
else:
    apply configured flatten policy
    mark lossy
```

---

# 28. Default conflict priority

First production policy should be deterministic and configurable.

Suggested default priority for overlapping source layers：

```text
1. highest visible surface by local height
2. if tied, preserve Water/Ocean over hazardous lower layer only when source geometry shows the latter is beneath it
3. otherwise preserve source instance order and emit high-severity diagnostic
```

Do not globally hard-code `water > magma`; actual stacked encounters may intentionally expose the hazardous surface.

A future source-layer semantic test corpus should refine this policy.

---

# 29. Validator

For every emitted MCLQ：

```text
payload size == 804
81 finite heights
minHeight <= maxHeight
64 flags
at least one flag != 0x0F
all visible low-nibble selectors in {0x01,0x04,0x06}
hidden selector == 0x0F
dark bit only emitted by current policy for Ocean/deep
flowCount == 0 for canonical first-pass output
MCNK liquid category bits agree with cells
```

Geometry consistency：

```text
for every visible cell, its 4 target corner vertices must be assigned
```

Round-trip through a target MCLQ parser before writing the final ADT.

---

# 30. Runtime fixtures

Minimum liquid corpus：

1. flat water, full 8x8.
2. partial water rectangle with exists mask.
3. ocean with per-cell deep bits.
4. LVF0 height+depth.
5. LVF2 depth-only / constant height.
6. magma/slime with UV data.
7. water + ocean non-overlapping in one MCNK.
8. two instances separated by hidden cells at different heights.
9. adjacent instances with matching shared edge height.
10. adjacent instances with conflicting edge height -> must diagnose.
11. overlapping vertical liquid layers -> must diagnose lossy flatten.
12. mixed magma/slime chunk -> must diagnose pending semantics.

Test in：

```text
Turtle WoW 1.18.1 real client
Tortoise map extractor
server swim/fatigue/liquid queries
```

---

# 31. Confidence table

| Rule | Confidence |
|---|---|
| Turtle target requires MCLQ | **real-client binary confirmed** |
| MCLQ 804-byte payload structure | client + Tortoise/TC source confirmed |
| flags low nibble used as cell type selector | **real-client binary confirmed** |
| hidden = 0x0F | client + extractor confirmed |
| Ocean selector = 0x01 | real-client render selector + legacy source correlation |
| Water selector = 0x04 | real-client render selector + legacy source correlation |
| Magma/Slime selector = 0x06 | real-client render selector + legacy source correlation |
| 0x80 = dark/deep water | extractor/source confirmed |
| water/ocean light low byte is shading/depth input | **real-client binary confirmed** |
| magma/slime light field = two u16 UVs | **real-client binary confirmed** |
| magma/slime UV client scale = 3/256 | **real-client binary confirmed** |
| LVF2 uses minHeightLevel as target flat height | source-format semantic confirmation |
| fishable not emitted into MCLQ in first pass | conservative policy; target equivalent unproven |
| zero legacy flow count is safe canonical baseline | strong inference, runtime fixture required |
| non-overlapping instances can be merged | derived directly from one-cell-selector/one-grid target representation |
| overlapping layers require flatten | target representability constraint |

---

# 32. Consequence

With this specification, the ADT client conversion's major format blockers are now sufficiently defined for implementation：

```text
MH2O -> MCLQ     frozen first-pass semantics
MCAL -> packed4  frozen
MCSH edges       frozen
holes            lossless direct semantic copy
WDT              straightforward regeneration
WDL              regenerate from terrain
```

Remaining work is predominantly implementation, fixtures, and target regression testing rather than unknown core binary layout.
