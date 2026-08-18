# ADT WotLK Normalization Checkpoint — 2026-08-18

状态：**source structural reader + terrain normalizer + semantic ADT normalizer implemented; latest commits require external CI/build verification**

目标：记录 `WoW 3.3.5a build 12340 ADT -> NormalizedAdt -> Turtle/Vanilla ADT` 当前实现边界，避免后续把 source-only MCNK 字段 raw-copy 到目标结构。

## Evidence levels

- **客户端二进制确认**：来自已上传 build12340 / Turtle 1.18.1 `WoW.exe` 的实际 loader 行为。
- **源码确认**：TrinityCore 3.3.5、Noggit3、Penqle/tortoise-wow 等实际 parser/writer。
- **强推断**：多方结构吻合但尚未完成客户端 fixture 回归。
- **未验证**：必须保留为 blocker/loss，不得 raw-copy。

## Implemented pipeline

```text
raw build12340 ADT
  -> WotlkAdtReader
  -> WotlkTerrainNormalizer
  -> Mh2oReader + LiquidTypeResolver
  -> WotlkAdtNormalizer
  -> NormalizedAdt
  -> TerrainWriter + LegacyLiquid
  -> McnkWriter
  -> AdtWriter
  -> AdtValidator
```

New code modules:

```text
include/turtle335/adt/NormalizedAdt.h
src/adt/NormalizedAdt.cpp

include/turtle335/adt/WotlkAdtReader.h
src/adt/WotlkAdtReader.cpp

include/turtle335/adt/WotlkTerrainNormalizer.h
src/adt/WotlkTerrainNormalizer.cpp

include/turtle335/adt/WotlkAdtNormalizer.h
src/adt/WotlkAdtNormalizer.cpp
```

Tests added:

```text
tests/test_normalized_adt.cpp
tests/test_wotlk_adt_reader.cpp
tests/test_wotlk_terrain_normalizer.cpp
tests/test_wotlk_adt_normalizer.cpp
```

## Critical MCNK source-field correction

Noggit3 build12340 `MapChunkHeader` provides the practical source interpretation:

```text
payload +0x40..0x4F = low_quality_texture_map[16]
payload +0x50..0x57 = disable_doodads_map[8]
...
payload +0x78        = unused1
payload +0x7C        = unused2
```

Therefore the source reader MUST NOT reinterpret those source bytes as old-target names merely because a legacy extractor struct labels the same byte positions as:

```text
predTex
nEffectDoodad
props
effectId
```

Current rule:

```text
source disableDoodadsMap != 0
 -> explicit Loss / future semantic downgrade
 -> target predTex = 0
 -> target nEffectDoodad = 0

source unused1/unused2 != 0
 -> explicit Loss / future semantic downgrade
 -> target props/effectId remain canonical defaults
```

Classification: **Noggit source-confirmed source interpretation; target mapping intentionally unverified and therefore not copied.**

## `MCNK +0x3E` correction

Build12340 real client terrain geometry uses the low 16-bit mask at `+0x3C` for coarse 4x4 holes. `+0x3E` is a distinct source field.

The target old struct has a 16-bit padding/legacy slot at the analogous location, but equality of byte position is not proof of semantic identity.

Current normalization policy:

```text
source holes low16 -> target holes
source +0x3E != 0  -> Loss diagnostic
target legacy3E    -> 0
```

## MCAL normalization

Source normalizer implements Noggit-compatible detection:

```text
MCLY 0x100 = uses alpha
MCLY 0x200 = compressed big-alpha

compressed            -> RLE -> 4096 uint8
uncompressed 4096 span -> big-alpha uint8
uncompressed 2048 span -> legacy packed 4-bit
other span             -> reject
```

Source WDT `MPHD & 0x4` is treated as a consistency hint; physical layer size/flags remain authoritative.

Old packed 4-bit source is first expanded and source edge-fix semantics are materialized when source MCNK bit15 is clear.

Then old sequential alpha maps are converted to independent contribution maps using the Noggit back-to-front rule:

```text
remaining = 255
for layer from top to bottom:
    contribution = round(oldSequential * remaining / 255)
    remaining -= contribution
```

Big/RLE source maps are already treated as independent contributions.

Mixed legacy-4bit and big/RLE storage inside one MCNK is currently rejected explicitly rather than guessed.

## Target bit15 correction

`ADT_MCAL_BINARY_ANALYSIS.md` already records Turtle real-client behavior:

```text
bit15 = 1 -> consume meaningful full 64x64 edge samples
bit15 = 0 -> synthesize row/column 63 from 62
```

The low-level writer previously always cleared bit15. This conflicted with the frozen binary-backed rule once MCAL had been normalized to a complete 64x64 semantic image.

Current writer contract:

```cpp
McnkTargetHeader::fullAlphaShadowEdges
```

- raw input `flags` bit15 is always cleared first: no source flag passthrough.
- when `fullAlphaShadowEdges == true`, target bit15 is set deliberately.
- `NormalizedAdt` uses `true` because `TerrainWriter` emits all normalized alpha samples; any supplied `targetMcsh` is contractually already full-edge target data.

This preserves the distinction between source physical flags and target semantic policy.

## WotlkAdtNormalizer ready/lossless policy

The normalizer uses Crucible-style explicit state:

```text
ready = false -> semantic blocker; target should not be published
lossless = false -> target can be generated, but a documented source feature is dropped/degraded
```

Current Blockers include:

- source high-resolution holes bit requiring unsupported target representation
- invalid MCRF placement indices
- MH2O without LiquidType.dbc resolver
- LiquidType resolver returns Unknown
- source liquid exists only as legacy MCLQ and has no explicit source-MCLQ semantic importer

Current Loss diagnostics include:

- nonzero source +0x3E
- nonzero source disable-doodads/detail bytes
- nonzero WotLK unused tail dwords
- unverified MCNK flag bits
- source MCSH not yet normalized
- source MCCV not yet normalized
- source MCSE not yet normalized
- source MFBO not yet normalized
- legacy MCLQ ignored when authoritative MH2O is present

## What is preserved automatically now

```text
MVER 18 structural validation
MTEX texture catalog
MMDX/MMID -> resolved M2 paths
MWMO/MWID -> resolved WMO paths
MDDF 36-byte placements
MODF 64-byte placements
MCRF semantic references
MCNK ix/iy
areaId
coarse 16-bit holes
low-quality texture map
impassible bit
MCVT absolute terrain heights
MCNR semantic normals
MCLY texture/effect/known animation-glow-reflection flags
MCAL old/big/RLE -> normalized alpha
MH2O -> normalized liquid layers through LiquidTypeResolver
```

## Immediate next implementation priorities

P0:
1. Obtain build/CI verification for the newly added Reader/Normalizer chain.
2. Add `LiquidType.dbc` source reader/resolver so MH2O no longer depends on a caller-supplied callback.
3. Add MCSH semantic normalizer and then remove `DroppedMcsh` for verified inputs.
4. Add source fixture(s) from a real build12340 client ADT and perform byte/semantic round-trip regression.

P1:
- MCCV normalization.
- MCSE policy.
- MFBO policy.
- WDT reader so `MPHD big-alpha` reaches the normalizer automatically.
- map-wide UID/path validation and LegacyAssetPathResolver.

Do not expand into M2/WMO implementation again until one real build12340 ADT passes the full source-reader -> normalized -> Turtle writer -> validator/client fixture path.
