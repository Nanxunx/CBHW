# WDT / WDL Binary Analysis and Target Strategy

状态：**Turtle target binary confirmed + 3.3.5/Tortoise source cross-check; WDL regeneration strategy frozen**

目标：为完整地图 retroport 确定 WDT/WDL 的目标写出规则，避免把 3.3.5a 的地图级扩展原样带入 Turtle 1.18.1。

---

## 1. WDT physical baseline

TrinityCore 3.3.5 与 Penqle/tortoise-wow 的 extractor 结构一致：

```text
MPHD payload = 8 x uint32 = 32 bytes
MAIN         = 64 x 64 entries
MAIN entry   = uint32 exist + uint32 data1 = 8 bytes
```

因此普通 tile-based map 的核心 WDT 结构没有跨版本尺寸障碍。

Tortoise 旧结构还保留 global-WMO 相关 `MWMO` 处理，用于旧式 WMO-only maps。

---

## 2. Turtle real-client WDT semantics

独立 Vanilla 1.12.1 客户端逆向中 WDT loader：

```text
0x694760
```

与 Turtle 1.18.1 同源代码路径吻合。

目标 loader 的重要行为：

```text
MVER
MPHD (0x20 bytes)
MAIN (64 x 64 x 8)
```

对 MPHD：

> 当前确认的运行时关键位是 `MPHD dword0 bit0`，用于表示 WMO-only / global-WMO map。

没有证据表明 Turtle terrain renderer 依赖 WotLK big-alpha `MPHD 0x4` 来决定 MCAL 解码；真实 alpha 解码由 MCNK/MCAL旧路径驱动。

---

## 3. Target MPHD policy

对于普通 ADT map：

```cpp
targetMphd.flags = sourceFlags & TargetAllowedMask;
```

第一版 canonical writer：

- 保留确有目标语义的 bit0 only when map is intentionally WMO-only.
- 清除 source big-alpha flag `0x4`，因为目标 MCAL 被统一写成 legacy packed 4-bit。
- 未确认的 WotLK-only bits 清除并记录 diagnostic。

建议：

```cpp
constexpr uint32_t TARGET_MPHDF_WMO_ONLY = 0x1;

target.flags = isGlobalWmoMap ? TARGET_MPHDF_WMO_ONLY : 0;
```

不要把源 MPHD flags 整字复制到目标。

---

## 4. MAIN regeneration

目标 `MAIN[64][64]` 应由转换任务实际输出的 ADT tile set重新生成，而不是复制 source bytes。

Normalized representation：

```cpp
struct MapTilePresence
{
    bool exists[64][64];
};
```

Target：

```cpp
entry.exist = exists[x][y] ? 1 : 0;
entry.data1 = 0;
```

若后续真实客户端/Vanilla corpus 证明第二 dword 有必须保留的目标语义，再扩展 mapper。

优点：

- 删除 source 中已跳过/转换失败 tile 时不会留下悬空 MAIN entry。
- 新增/重排 tile 时保持 WDT 与输出文件集合一致。

---

## 5. Global-WMO maps

WMO-only maps 必须与普通 ADT maps 分开处理。

Target writer 应明确建模：

```cpp
enum class MapLayoutKind
{
    TerrainTiles,
    GlobalWmo
};
```

GlobalWmo 路径需要单独生成/保留：

```text
WDT global WMO name/reference chunks
placement / MODF-style definition where target format requires
MPHD bit0
```

第一版普通地图转换器不应把 global-WMO map 错当成 4096 tile terrain map。

---

# WDL

## 6. Turtle target WDL layout

Vanilla/Turtle low-resolution distant terrain使用：

```text
MVER
MAOF
MARE-like per-tile low-resolution height records
```

核心索引：

```text
MAOF = 64 x 64 absolute file offsets
```

每个存在 tile 的核心低分辨率高度数据：

```text
545 x int16
```

分为：

```text
17 x 17 outer grid = 289
16 x 16 inner grid = 256
---------------------------
                         545
```

Turtle/Vanilla client uses these values for distant/horizon terrain representation, not authoritative gameplay terrain height.

---

## 7. Why target WDL should be regenerated

3.3.5a WDL loader contains additional map/global-object handling compared with the simplest Vanilla path.

There is no need to preserve source WDL byte-for-byte because the converter already has authoritative normalized ADT terrain heights.

Recommended architecture：

```text
NormalizedADT map
       ->
WdlGenerator
       ->
Turtle/Vanilla WDL
```

This avoids an unnecessary cross-version WDL parser/writer compatibility dependency.

---

## 8. Deriving the 17x17 outer grid

One ADT tile contains：

```text
16 x 16 MCNK chunks
```

Each MCNK contributes terrain corner positions.

Construct a tile-level `17 x 17` grid by selecting the shared corner height at each MCNK boundary：

```text
(0,0) ... (16,0)
  .           .
  .           .
(0,16)...(16,16)
```

Because adjacent MCNKs share boundary terrain positions, the generator should validate continuity and choose a deterministic owner (for example north/west chunk) when reading a shared point.

If duplicate shared vertices differ beyond tolerance, emit a terrain seam warning instead of silently averaging.

---

## 9. Deriving the 16x16 inner grid

Each MCNK's 145-height topology contains the center/interior vertex corresponding to that chunk's coarse low-resolution center sample.

For each of the 256 MCNKs：

```text
select the target low-resolution center height
 -> inner[y][x]
```

Thus：

```text
16 x 16 = 256 inner samples
```

The exact MCVT index for the center sample must be encoded in a tested helper based on the 9/8 staggered MCNK vertex topology; do not scatter magic indices throughout the writer.

---

## 10. Height quantization

WDL stores signed 16-bit values, while normalized terrain uses float world heights.

Writer must define a deterministic conversion：

```cpp
int16_t QuantizeWdlHeight(float worldHeight);
```

Requirements：

- finite input only
- explicit rounding mode
- clamp/diagnose outside int16 representable range
- use the same coordinate/height convention expected by Turtle loader

Do not reinterpret float bits or truncate without range checking.

Before final implementation, derive the exact Vanilla WDL scale/origin from real-client consumer and known-good WDL corpus. Until then, retain the generator interface but mark quantization implementation as requiring fixture validation.

---

## 11. MAOF writer

Use two-pass serialization：

```text
Pass 1:
  reserve MAOF[4096]

Pass 2:
  for each present tile:
      align if target requires
      record absolute file offset
      write tile low-res height record

Final:
  backpatch MAOF
```

Absent tile：

```text
MAOF entry = 0
```

MAOF and WDT MAIN must be generated from the same `MapTilePresence` object.

---

## 12. Validation

### WDT

```text
- MPHD exactly target size
- MAIN exactly 4096 entries
- each MAIN present tile has a converted ADT
- no ADT output exists with MAIN absent unless explicitly excluded
- WMO-only bit agrees with layout kind
```

### WDL

```text
- MAOF contains 4096 entries
- absent tile offset == 0
- each non-zero offset lies inside file
- each generated terrain tile supplies exactly 545 low-res heights
- all quantized heights in int16 range
```

Cross-validation：

```text
WDT MAIN presence
=
WDL MAOF presence
=
converted ADT tile set
```

for ordinary terrain maps.

---

## 13. Implementation modules

```text
map/
├─ MapTilePresence
├─ Wdt335Reader          # minimal source semantics only
├─ VanillaWdtWriter
├─ WdlGenerator
├─ VanillaWdlWriter
└─ MapLevelValidator
```

WDL does not need a full source-preserving converter for the first production path.

---

## 14. Confidence

| Finding | Confidence |
|---|---|
| WDT MPHD = 8 dwords | TC 3.3.5 + Tortoise source confirmed |
| WDT MAIN = 64×64×8 | TC 3.3.5 + Tortoise source confirmed |
| Turtle target uses MPHD bit0 for WMO-only | Vanilla/Turtle binary lineage confirmed |
| Turtle does not need MPHD 0x4 for target MCAL decode | Real target alpha loader evidence, high confidence |
| WDL MAOF 64×64 | Vanilla client reverse-engineering confirmed |
| WDL low-res tile = 545 s16 | Vanilla target reverse-engineering confirmed |
| regenerate WDL from normalized ADT | Architecture rule, high confidence |
| exact float→WDL int16 scale/rounding | **still requires fixture/binary consumer validation** |

---

## 15. Next implementation consequence

WDT/WDL no longer block the core ADT converter.

The remaining first-pass client ADT implementation work is concentrated in：

```text
MH2O -> MCLQ
MCAL 8-bit/RLE -> packed 4-bit
MCNK subchunk serialization / offset rebuild
MMDX/MMID + MWMO/MWID table rebuild
MDDF/MODF placement consistency
```

`holes` and ordinary WDT tile layout are now compatibility-preserving rather than lossy conversion problems.
