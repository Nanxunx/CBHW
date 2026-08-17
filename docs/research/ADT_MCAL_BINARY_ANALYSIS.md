# ADT MCAL Binary Analysis — 3.3.5a to Turtle/Vanilla

状态：**source-confirmed input formats + Turtle real-client output behavior confirmed**

目标：冻结 `Turtle335Converter` 第一版 `MCAL` 转换规则，避免 3.3.5a 地图在 Turtle 1.18.1 中出现地表纹理黑块、混合错误和 chunk 边缘接缝。

---

## 1. Final result

3.3.5a 输入端必须支持三种 alpha map 表达：

```text
A. legacy packed 4-bit        2048 bytes/layer
B. big alpha raw 8-bit       4096 bytes/layer
C. big alpha RLE compressed  variable bytes -> 4096 bytes decoded
```

Turtle/Vanilla 目标统一写出：

```text
legacy packed 4-bit
64 x 64 samples
2048 bytes per alpha layer
low nibble first
```

目标 MCNK：

```text
bit15 (do_not_fix_alpha_map / alpha-full) = 1
```

前提：Writer 在输出前必须把 **MCAL 与 MCSH 都归一化成完整 64x64 边缘数据**。

---

## 2. 3.3.5 source big-alpha selection

Noggit source cross-check:

```text
wowdev/noggit3
src/noggit/map_index.cpp
```

WDT `MPHD.flags`：

```cpp
mBigAlpha = (mphd.flags & 4) != 0;
```

因此：

```text
MPHD bit 0x4 = map uses big alpha
```

该 map-level 状态传给每个 `MapTile` / `MapChunk`。

证据等级：**source confirmed**。

---

## 3. Per-layer MCLY flags

Noggit definitions：

```cpp
FLAG_USE_ALPHA        = 0x100
FLAG_ALPHA_COMPRESSED = 0x200
```

对每个 MCLY layer：

```text
flags & 0x100 -> layer has alpha map
flags & 0x200 -> big-alpha RLE compressed
```

注意：base layer（layer 0）通常没有自己的 alpha map；最多 4 个 texture layers，因此最多 3 个 alpha maps。

---

## 4. Robust source format detection

Noggit `TextureSet` 不只相信 WDT flag，还检查实际 layer byte span：

```cpp
use_big_alphamaps =
       (layer_size == 4096)
    || (_layers_info[layer].flags & 0x200);
```

因此 Reader 应采用相同容错思想：

```text
if MCLY.flags & 0x200:
    decode as RLE big alpha
else if layer span == 4096:
    decode as raw big alpha
else if layer span == 2048:
    decode as legacy packed 4-bit
else:
    diagnose malformed/unknown MCAL
```

WDT MPHD bit 0x4 仍作为地图级预期格式提示，但实际尺寸优先用于损坏/错误标志容错。

---

## 5. RLE big-alpha format

Noggit `Alphamap::readCompressed()` source-confirmed format：

每个 run header：

```text
bit7      = mode
bits0..6  = count
```

模式：

```text
mode = 0 -> copy next `count` bytes
mode = 1 -> repeat next single byte `count` times
```

解压终点：

```text
4096 bytes = 64 x 64 uint8 alpha
```

Converter requirements：

- reject/degrade invalid run that exceeds 4096 output samples
- detect premature EOF
- reject zero-progress malformed streams
- record consumed source byte count for layer-span validation

Noggit encoder also avoids crossing 64-pixel row boundaries; Reader should not require that property but Validator may warn if an input run crosses a row.

---

## 6. Raw big alpha

Source format：

```text
64 x 64 x uint8
= 4096 bytes
```

No unpacking needed：

```text
NormalizedAlpha[y][x] = sourceByte
```

---

## 7. Legacy packed 4-bit source

Physical size：

```text
64 x 64 / 2
= 2048 bytes
= 0x800
```

Nibble order：

```text
pixel 0 = low nibble
pixel 1 = high nibble
```

Expand to normalized 8-bit：

```cpp
uint8_t Expand4To8(uint8_t n)
{
    return uint8_t(n * 17); // n | (n << 4)
}
```

Noggit source uses exactly this expansion.

---

## 8. Turtle real-client target decoder

Turtle WoW 1.18.1 real `WoW.exe` alpha family：

```text
0x6B0350
0x6B0770
0x6B0A60
0x6B0AC0
0x6B0BF0
```

The renderer consumes legacy **4-bit packed** alpha input.

No WotLK 4096-byte big-alpha / RLE client input path has been identified in the Turtle terrain loader.

Therefore target client ADT must not retain WotLK big-alpha encoding.

---

## 9. MCNK bit15 is binary-confirmed

At Turtle function:

```text
0x6B0A60
```

client loads MCNK flags and executes a sign-bit test on the high byte (`CH`). This is MCNK flags bit15.

### bit15 = 1

Path：

```text
0x6B0AC0
```

loops full：

```text
64 rows x 64 columns
```

and consumes the packed alpha samples without synthesizing the last edge.

### bit15 = 0

Path：

```text
0x6B0BF0
```

loops：

```text
63 x 63
```

then explicitly：

```text
last column <- previous column
last row    <- previous row
last corner <- previous corner
```

Therefore：

> **MCNK bit15 means the last row/column are meaningful / do not apply the legacy alpha edge fix.**

This is real-client binary confirmed.

---

## 10. Why target Writer should set bit15

The converter starts from a normalized 64x64 alpha image, including fully reconstructed source edges.

If target bit15 were left clear, Turtle would overwrite valid edge samples by duplicating row/column 62.

Therefore first-pass target policy：

```cpp
targetMcnk.flags |= (1u << 15);
```

and write all 4096 logical samples into packed 4-bit MCAL.

---

## 11. MCSH interaction — important

Noggit uses the same MCNK `do_not_fix_alpha_map` bit when loading `MCSH` shadow maps.

When bit15 is clear it duplicates the last row/column of the 64x64 one-bit shadow map as well.

Therefore setting target bit15 requires the Writer to normalize `MCSH` first.

Recommended pipeline：

```text
Source MCAL
 -> decode to full 64x64 uint8
 -> if source legacy edge-fix semantics: materialize fixed last row/column

Source MCSH
 -> decode 64x64 bit mask
 -> if source legacy edge-fix semantics: materialize fixed last row/column

then
 -> set target MCNK bit15
 -> write packed target MCAL
 -> write full target MCSH
```

This keeps alpha and terrain shadow edge semantics synchronized.

---

## 12. 8-bit -> 4-bit quantization

Turtle's 4-bit value expands to：

```text
0, 17, 34, ... 255
```

Nearest representable quantization is therefore：

```cpp
uint8_t Quantize8To4(uint8_t alpha)
{
    return uint8_t((uint32_t(alpha) + 8u) / 17u);
}
```

clamped to `[0,15]`.

This is preferred over plain `alpha >> 4` because it minimizes absolute reconstruction error under the target client's `nibble * 17` expansion.

For regression testing retain a selectable `truncate` mode, but default should be nearest rounding.

---

## 13. Target packing

For each non-base alpha layer：

```cpp
for (size_t i = 0; i < 4096; i += 2)
{
    uint8_t lo = Quantize8To4(alpha[i]);
    uint8_t hi = Quantize8To4(alpha[i + 1]);
    out[i / 2] = lo | (hi << 4);
}
```

Output：

```text
2048 bytes per layer
```

low nibble first.

---

## 14. MCLY / MCAL rebuilding

Do not preserve source `ofsAlpha` after conversion.

For target layers：

```text
layer0: no alpha map required
layer1: ofsAlpha = 0
layer2: ofsAlpha = 2048
layer3: ofsAlpha = 4096
```

for layers that actually carry `FLAG_USE_ALPHA`.

More generally calculate offsets while serializing, because malformed/custom files may omit maps.

Target MCLY rules：

```text
FLAG_USE_ALPHA (0x100) retained where layer has alpha
FLAG_ALPHA_COMPRESSED (0x200) cleared
```

Target `MCAL` chunk payload is the concatenation of uncompressed packed 4-bit maps.

`MCNK.ofsMCAL` and `MCNK.sizeMCAL` must be rebuilt.

---

## 15. WDT target rule

Source WDT `MPHD.flags & 0x4` describes big-alpha source data.

Target Turtle files are deliberately written as legacy packed 4-bit MCAL.

Therefore target WDT writer should **clear the big-alpha flag** unless future Turtle binary evidence proves a separate reason to retain it：

```cpp
targetMphd.flags &= ~0x4u;
```

This rule is currently **source/format-consistent and strongly recommended**; exact Turtle WDT consumption of MPHD bit0x4 should still be recorded separately in the WDT binary-analysis phase.

---

## 16. Normalized representation

```cpp
struct NormalizedAlphaMap
{
    std::array<uint8_t, 64 * 64> alpha;
};

struct NormalizedTextureLayer
{
    uint32_t textureId;
    uint32_t sourceFlags;
    uint32_t effectId;
    std::optional<NormalizedAlphaMap> alpha;
};
```

No source MCAL byte offset or compression state survives into the normalized representation.

Optional diagnostics metadata may retain：

```text
sourceEncoding = Legacy4 / Big8 / Rle8
sourceByteSpan
sourceUsedEdgeFix
```

for reporting only.

---

## 17. Converter implementation skeleton

```cpp
NormalizedAlphaMap DecodeSourceAlpha(
    ByteSpan src,
    uint32_t mclyFlags,
    bool mapBigAlpha,
    bool sourceDoNotFixEdge);

void NormalizeLegacyAlphaEdge(
    NormalizedAlphaMap& alpha);

void NormalizeLegacyShadowEdge(
    NormalizedShadowMap& shadow);

std::array<uint8_t, 2048> EncodeVanillaAlpha4(
    NormalizedAlphaMap const& alpha);
```

Pipeline：

```text
3.3.5 MCLY/MCAL
   -> detect encoding
   -> decode to uint8[4096]
   -> materialize edge semantics
   -> quantize to 4-bit
   -> pack low-nibble-first
   -> rebuild MCLY offsets
   -> rebuild MCAL
   -> set MCNK bit15
```

---

## 18. Validation invariants

For every MCNK：

```text
1 <= nLayers <= 4
alpha map count <= 3
all target alpha maps exactly 2048 bytes
no target MCLY layer has 0x200 compression flag
MCLY.ofsAlpha lies within MCAL payload
MCAL total size agrees with emitted maps
bit15 is set when full edges are serialized
MCSH edge semantics have been normalized consistently
```

Round-trip validator：

```text
source alpha -> normalized8 -> target4 -> Turtle-equivalent expand8
```

report：

```text
max absolute error
mean absolute error
changed pixel count
edge mismatch count
```

Expected quantization maximum error with nearest mapping：

```text
<= 8 alpha units
```

---

## 19. Confidence

| Finding | Confidence |
|---|---|
| WDT MPHD 0x4 selects big alpha | Source confirmed |
| MCLY 0x100 alpha-present | Source confirmed |
| MCLY 0x200 RLE big-alpha | Source confirmed |
| RLE run format | Source confirmed |
| raw big alpha = 4096B | Source confirmed |
| legacy alpha = 2048B 4-bit | Source + Turtle binary confirmed |
| low nibble first | Source + Turtle binary confirmed |
| Turtle bit15=full 64x64/no edge fix | **Turtle binary confirmed** |
| target set bit15 after normalization | Derived conversion rule, high confidence |
| clear target WDT big-alpha flag | Strongly recommended; WDT client consumption pending explicit binary note |

---

## 20. Next blocker

After MCAL, the next ADT client-level compatibility item is：

```text
WDT / WDL + target MPHD flags
```

followed by：

```text
MCNK holes 32-bit WotLK -> 16-bit Vanilla mapping
```

The holes conversion must not be implemented as a truncating cast until the actual spatial bit mapping is proven.
