# WoW-Crucible ADT Placement Audit — 2026-08-18

目标：评估 `Zaelemgaad/WoW-Crucible` 对 Turtle335Converter 的 ADT object-placement、UID、MCRF、bounds 和 full-ADT writer 的可借鉴价值。

证据等级：本文除明确标记的建议外均为 **源码确认**。

## 1. 总结

WoW-Crucible 不是只有 UI/DBC 编辑器。当前 `src/WoWCrucible.Core/` 已包含一套相当完整的 WotLK ADT placement 编辑管线：

```text
AdtPlacementLifecycleService
AdtPlacementTransformService
AdtMultiTilePlacementService
AdtMultiTilePlacementTransformService
M2PlacementBoundsService
WmoPlacementBoundsService
MapAssetInspectionService
```

对 Turtle335Converter 最有价值的不是它的界面，而是以下算法：

1. `MMDX/MMID/MDDF` 与 `MWMO/MWID/MODF` 的一致性维护；
2. MDDF=36 bytes、MODF=64 bytes 的 record rebuild；
3. placement insert/delete 后的 MCRF per-cell index rebuild；
4. 删除 placement 时所有更高索引的 MCRF reference 自动减 1；
5. MCNK 内嵌 chunk 大小变化时的 offset shifting；
6. root chunk 重排后统一 backpatch MHDR；
7. 256-entry MCIN 根据新 MCNK byte positions/lengths 重写；
8. map-wide UniqueID occupancy；
9. object bounds → touched MCNK/tile selection；
10. 跨 tile placement 作为一个协调事务发布。

项目许可证为 MIT，对算法借鉴/部分代码复用比 GPL 项目友好。

---

## 2. Placement record layout

`AdtPlacementLifecycleService.BuildRecord()` 直接构建：

### M2 / MDDF

```text
36 bytes
+0x00 uint32 NameId
+0x04 uint32 UniqueId
+0x08 vec3 Position
+0x14 vec3 Rotation
+0x20 uint16 ScaleRaw
+0x22 uint16 Flags
```

### WMO / MODF

```text
64 bytes
+0x00 uint32 NameId
+0x04 uint32 UniqueId
+0x08 vec3 Position
+0x14 vec3 Rotation
+0x20 vec3 MinimumExtent
+0x2C vec3 MaximumExtent
+0x38 uint16 Flags
+0x3A uint16 DoodadSet
+0x3C uint16 NameSet
+0x3E uint16 ScaleRaw
```

这与 Trinity/Tortoise 之前确认的 36/64 byte source placement layout 一致。

---

## 3. MMDX/MMID and MWMO/MWID path catalogs

`ResolveNameId()`：

```text
M2:
MMDX string table + MMID offset table

WMO:
MWMO string table + MWID offset table
```

行为：

1. 先按 existing offset index 查找同路径；
2. 如果字符串已存在但没有 index，复用 string offset；
3. 如果字符串不存在，append nul-terminated string；
4. append 新 MMID/MWID offset；
5. MDDF/MODF `NameId` 使用 index-table position，而不是字符串 byte offset。

推荐 Turtle335Converter 在 NormalizedADT 中也把：

```text
asset path identity
```

与：

```text
serialized NameId / string offset
```

分离，Writer 最后统一分配。

---

## 4. MCRF rebuild

Crucible 的处理值得直接借鉴。

每个 MCNK 的 MCRF payload 被视为：

```text
M2 refs first  : nDoodadRefs × uint32
WMO refs next  : nMapObjRefs × uint32
```

Add：

- placement 与该 cell bounds 相交时，将新 placement index append 到对应 M2/WMO refs；
- 避免 duplicate reference。

Delete：

```text
ref == deletedIndex -> remove
ref > deletedIndex  -> ref - 1
ref < deletedIndex  -> unchanged
```

这是因为 MDDF/MODF table 删除一条以后，后续 placement index 全部左移。

Writer 随后：

- rebuild complete MCRF chunk；
- 更新 `nDoodadRefs` / `nMapObjRefs`；
- 若 MCRF 大小改变，shift MCNK 中所有后续 subchunk offsets；
- rebuild MCNK chunk size。

这应直接进入 Turtle335Converter 的 placement/index validator。

---

## 5. MHDR and MCIN full backpatch

`RewriteTopLevelReferences()` 是本项目 full ADT Writer 很好的独立实现参考。

流程：

```text
recalculate every top-level chunk absolute position
      ↓
MHDR target fields
      ↓
write offset relative to MHDR payload base
      ↓
MCIN[256]
      ↓
map original MCNK identity to new absolute position
      ↓
write new MCNK offset + byte size
```

它明确维护的 MHDR targets：

```text
MCIN
MTEX
MMDX
MMID
MWMO
MWID
MDDF
MODF
MFBO
MH2O
MTXF
```

对于我们的 Vanilla target，不能原封不动复制这组字段（例如最终 target client不保留 MH2O），但**两遍 layout/backpatch 方法完全适用**。

---

## 6. UniqueID policy

Crucible 不把 UniqueID 当 tile-local ID。

它会扫描同一 map workspace 的所有有效 ADT：

```text
all MDDF.UniqueId
+
all MODF.UniqueId
```

形成一个共享 occupancy namespace。

新增 placement：

```text
uid = max(existing M2/WMO UID across map) + 1
```

除非用户显式指定一个已验证未占用的 UID。

此外：

- UID=0 被拒绝；
- M2/WMO 间 UID collision 被认为是 ambiguous/corrupt；
- deletion 会按 UID 在所有相关 tiles 找到同一语义 placement；
- 同一 tile 两个相同 UID 记录会阻止自动删除。

推荐 Turtle335Converter 建立：

```text
MapPlacementUidRegistry
```

而不是在转换每个 ADT 时独立分配 UID。

对于“纯 retroport 不新增对象”，应保留 source UID；对 repair/remap collision，必须 map-wide deterministic remap。

---

## 7. Bounds-based MCNK references

新增对象不是简单加入“位置所在 cell”。Crucible先算对象 world-space AABB，再决定所有被 AABB 覆盖的 cells。

### M2

`M2PlacementBoundsService` 对 WotLK v264：

```text
vertex stride = 48
header +0x3C = vertex count
header +0x40 = vertex offset
```

扫描所有 vertex positions 得到 local-space min/max，再应用 placement position/orientation/scale 得 world AABB。

### WMO

`WmoPlacementBoundsService` 从 version-17 WMO root `MOHD` 读取 authoritative local bounds：

```text
MOHD payload +36 = min
MOHD payload +48 = max
```

将 8 个 AABB corners 按 placement transform 转换，得到 MODF world-space extents。

特别值得保留的安全规则：

- 仅平移已有 WMO 时，可以给已有 MODF min/max 加相同 delta；
- 改 rotation/scale 时，必须有 exact WMO root；
- root hash/bounds 必须能重现当前 MODF extents，之后才允许重算新的 extents。

这避免因为引用了错误版本 WMO root 而写出错误 MODF bounds。

---

## 8. Multi-tile placement transaction

大型 M2/WMO 的 world AABB 可以跨 ADT tile。Crucible 将这些重复 placement records 视为一个事务：

```text
object world AABB
 -> TouchedTiles()
 -> every touched tile must exist
 -> same UID used in every segment
 -> tile-local NameId/index/MCRF rebuilt independently
 -> stage all output ADTs in temp tree
 -> validate all
 -> atomically publish Payload tree + patch manifest + receipt
```

如果某个需要的 tile 不存在：

```text
BLOCK
```

而不是只发布部分对象。

如果任何 segment 失败，临时输出整体删除。

这是非常值得引入 Turtle335Converter 的地图批处理层的事务语义。

---

## 9. Hash-bound plans and post-write verification

Crucible 不仅写文件，还绑定：

```text
source ADT SHA256
asset M2/WMO SHA256
map workspace fingerprint
UID occupancy fingerprint
planned references
bounds evidence
```

Apply 时重新验证，写完后再次解析 output，确认：

- placement table count/identity；
- transformed position/orientation/scale；
- WMO extents；
- MCRF affected-cell set；
- untouched records未改变。

推荐我们的批量 converter 使用类似：

```text
ConversionManifest
InputHashes
LossReport
OutputHashes
ValidationResult
```

这比“运行结束打印 success”可靠得多。

---

## 10. What NOT to reuse unchanged

Crucible placement code当前针对 **WotLK MVER18 ADT editor**，不是 Vanilla target writer。

不能直接复制的部分：

- target仍保留 WotLK MHDR/MH2O/MTXF assumptions；
- 它修改现有 WotLK raw chunks，而我们需要 semantic 335 -> Vanilla rewrite；
- raw chunk order/offset handling应服从 Turtle target loader与我们自己的 writer；
- client placement conversion还要配合 converted M2/WMO asset paths。

因此正确方式：

```text
借鉴 algorithms / invariants / transaction model
+
在 NormalizedADT/VanillaAdtWriter 中独立实现
```

---

## 11. StaticM2DownportService audit

Crucible 另有约 82KB 的 `StaticM2DownportService`，工程质量值得参考，但目标不是本项目当前跨度：

```text
input: modern MD21 / MD20 v274
output: WotLK MD20 v264
```

它采用很好的“显式支持边界”原则：

- unknown/non-neutral chunk -> blocker；
- 不支持的 shader/blend/track -> blocker/loss；
- 输出 plan 中记录 `Transformations / Losses / Blockers`；
- source hashes 与 listfile texture resolution 都进入计划。

推荐将这种 loss-accounting 模型移植到我们的 264 -> 256 pipeline；不要拿其字段转换逻辑作为 Vanilla writer。

---

## 12. License

WoW-Crucible 本体为 MIT License（2026 Zaelemgaad and contributors）。

因此：

- 可以借鉴实现；
- 可以在遵守 MIT copyright/license notice 的前提下复制/修改部分代码；
- 但项目仍应优先保持我们自己的 target-specific clean architecture，而非引入大型 C# dependency。

---

## 13. Recommended architecture changes

加入：

```text
NormalizedADT
 ├─ ModelPathCatalog
 ├─ WmoPathCatalog
 ├─ M2Placements
 ├─ WmoPlacements
 └─ Cells[].placement references

AdtPlacementPlanner
 ├─ preserve/remap UID
 ├─ calculate/reuse bounds
 ├─ intersect MCNK cells
 └─ map-wide validation

VanillaAdtWriter
 ├─ MMDX/MMID
 ├─ MWMO/MWID
 ├─ MDDF/MODF
 ├─ per-MCNK MCRF
 ├─ MCNK writer
 ├─ MCIN backpatch
 └─ MHDR backpatch

MapPlacementUidRegistry
ConversionManifest
```

---

## 14. Updated priority

**P0**: full ADT root writer should now be implemented using semantic placement tables rather than merely copying source MMDX/MMID/MDDF/MODF chunks.

Order:

```text
1. build canonical M2/WMO path catalogs
2. serialize MMDX/MMID + MWMO/MWID
3. serialize MDDF/MODF preserving source UID and transform
4. rebuild per-cell MCRF from normalized references/bounds
5. serialize all 256 MCNK
6. write MCIN
7. write top-level chunks
8. backpatch MHDR/MCIN
9. reopen and validate
```

**P1**: add map-wide UID collision validator and bounds validator.

**P1**: after first full ADT fixture passes, return to M2 264->256 and audit actual jM2lib/M2Lib + M2Workshop.
