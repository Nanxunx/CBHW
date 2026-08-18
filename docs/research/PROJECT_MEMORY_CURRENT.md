# 模型移植项目 — 当前权威记忆

更新时间：2026-08-18

本文件是 `NansenCore/Turtle335Converter` 当前优先读取的项目记忆。旧的 `PROJECT_MEMORY_2026-08-18.md` 保留为历史记录；若两者冲突，以本文件、最新专题文档、真实客户端二进制和当前源码为准。

## 当前用户指令

- 本项目属于“模型移植”长期项目，后续相关对话继续沿用本文件中的证据、架构和实现进度。
- 用户要求“继续”时直接推进，不重复询问已经确定的目标和基础约束。
- 当前最高优先级仍是**真实 WoW 3.3.5a build12340 fixture 验证**；没有真实 `ADT/WDT/LiquidType.dbc` 时，不把时间耗在 speculative chunk 上，应优先完善真实 fixture 扫描/诊断/批处理能力和工程验证。
- 2026-08-18 已搜索当前会话上传与 File Library，没有找到可直接作为真实 build12340 `ADT/WDT/LiquidType.dbc` fixture 的文件。

## 目标

```text
WoW 3.3.5a build 12340
 -> semantic retroport
Vanilla 1.12.x / Turtle WoW 1.18.1 build 7272
```

原则：Reader -> Normalized semantic model -> semantic downgrade -> target Writer -> validator -> real-client/server regression。禁止只改版本号、跨世代 raw memcpy、同 offset=同语义、删除块后沿用旧 offset。

## 客户端基线

- `Wow(1).exe`：3.3.5a build12340 原始分析基准。
- `wow14.exe`：同 build，仅有 Load Local Files 调试补丁。
- Turtle 1.18.1 `WoW.exe`：Vanilla 1.12.1 build5875-derived、Turtle build7272 32-bit MPQ 客户端。
- 目标兼容最终裁判：Turtle 真客户端 loader + Penqle/tortoise-wow 目标工具链。

## M2 保留结论

客户端二进制确认：

```text
WotLK MD20 = 264
Turtle accepts = 256/257
production target = 256
Skin 48B -> embedded View 44B
Submesh 48 -> 32
Animation 64 -> 68
Bone 88 -> 108
Ribbon 176 -> 220
Particle 476 -> 504
Vertex 48 -> 48
```

M2 必须做 external SKIN->embedded View、legacy track rebuild、quaternion conversion、Ribbon/Particle downgrade；不能只改 version。主要算法参考 Koward/M2Lib；M2Workshop 用于 repair/regression；warcraft-rs/wow-m2 主要用作 parser/struct reference。

## WMO 保留结论

- MVER=17 两边都有，不能区分世代。
- MOPY bit 语义必须 semantic remap。
- WotLK MOGP `0x01000000`=第二 MOCV，`0x02000000`=第二 MOTV；Turtle old loader不消费。
- MOMT 64B 两边同宽但 shader/material 语义不可 raw copy。
- WotLK shader6 是第二 UV/第二 vertex-color 明确边界；Turtle 已确认 0..5 路径。
- MOHD 最后 DWORD 是 flags；旧 Tortoise `liquidType` 名称不可信。

## ADT 客户端/服务器分流

客户端：

```text
335 MH2O -> normalized liquid -> legacy multi-record MCLQ -> Turtle ADT
```

服务器 `.map`：

```text
original 335 MH2O -> Trinity-derived reader semantics -> Tortoise z1.4 writer
```

禁止从 retroported client MCLQ 重新抽服务器 map。

## MCLQ multi-record

Turtle 真客户端确认固定 category 顺序：

```text
0x04 River/Water
0x08 Ocean
0x10 Magma
0x20 Slime
```

每类 804B record，顺序 River->Ocean->Magma->Slime；完整区域 `8 + 804*N`。

重要：legacy `QLCM` 的**inner chunk size 可以是 0**，真实总长度由 `MCNK.sizeMCLQ` 持有。因此 Reader/Normalizer/Probe 都必须使用 owning MCNK size 识别完整 MCLQ block，不能用通用 `innerSize+8` 把块误截成 8B。

cell code：

```text
Ocean 0x01
Slime 0x03
Water 0x04
Magma 0x06
Hidden 0x0F
bit6 fishable
bit7 fatigue/deep
```

当前代码已按 category group 输出；cross-category overlap 可保留，same-category overlap/height conflict 报 lossy。

## MCAL + bit15 当前最终规则

Source：

```text
MCLY 0x100 USE_ALPHA
MCLY 0x200 COMPRESSED
2048B old packed4
4096B big8
compressed RLE8
WDT MPHD&0x4 = big-alpha hint
```

Normalizer先还原完整64x64，再将 old sequential alpha 转成 independent contributions；mixed old4+big/RLE 暂时拒绝。

**Target bit15 最终规则：**

```text
bit15=1 -> Turtle使用完整64x64 alpha/shadow边缘
bit15=0 -> Turtle从62复制生成row63/col63
```

因此 source raw bit15 不 passthrough；Normalizer先物化完整边缘，production target `fullAlphaShadowEdges=true`，Writer显式设置 target bit15。

结构 Validator **允许 bit15=0 和 bit15=1 两种合法客户端解码模式**；production 策略由 `test_normalized_adt` 单独锁死为 bit15=1。任何旧文档中“canonical target总是bit15=0”的描述均已过时。

## MCSH

Noggit确认 payload=512B=64*uint64；source bit15=0 时 col63<-62、row63<-62、corner<-62,62。`WotlkMcshNormalizer` 已实现；全零 shadow 自动省略。Target 使用完整边缘+bit15=1。

## MCNR

Noggit保存模型：MCNR declared payload=435B (145*3)，随后有13B extra bytes在 chunk 外，再开始MCLY。当前 Reader/Writer按此实现。

## MCCV

Production 目前**不支持直接保留**。Turtle MCNK pointer-fixup `0x6AF970`处理 MCVT/MCNR/MCLY/MCRF/MCAL/MCSH/MCSE/MCLQ，但没有建立 `offsMCCV (+0x74)` runtime pointer。`NormalizedAdt` 已移除 `targetMccv` production field。Source MCCV继续报告 Loss/Risk。

## Source MCNK 字段修正

Noggit build12340 source：

```text
+0x40..0x4F low_quality_texture_map[16]
+0x50..0x57 disable_doodads_map[8]
+0x78 unused1
+0x7C unused2
```

禁止把这些 raw-copy 到老 target 名称 predTex/nEffectDoodad/props/effectId。非零 source 值->Loss，target canonical defaults。

`+0x3C`低16位是真客户端 coarse 4x4 hole mask；`+0x3E`独立字段，非零->Loss，target=0。High-res holes bit16当前Blocker。

## 当前 ADT 实现

当前仓库已存在：

```text
WotlkAdtReader
WotlkAdtProbe
WotlkTerrainNormalizer
WotlkMcshNormalizer
Mh2oReader
WotlkAdtNormalizer
NormalizedAdt
TerrainWriter
LegacyLiquid
McnkWriter
PlacementWriter
AdtWriter
AdtValidator
WotlkWdtReader
LiquidTypeDbc
```

Reader解析 root/path catalogs/placements/256 MCNK/MCRF/raw terrain chunks/MH2O/MFBO，并已修正 legacy MCLQ：按 `MCNK.sizeMCLQ` 复制完整 owning block；inner size=0 合法。

`LiquidTypeDbc`按 Trinity build12340 规则：field0=ID，field3=SoundBank；0 Water/1 Ocean/2 Magma/3 Slime。

`WotlkWdtReader`解析 MVER18、MPHD、MAIN；提供 globalWmo 与 bigAlpha；terrain-only target保留tile presence并清 source feature flags。

### Windows 内存/栈安全修正

`NormalizedAdt` 和 `AdtWriterInput` 曾用 `std::array<...,256>` 直接内嵌大型 cell，真实调用链会在 Windows 默认 1MiB stack 下产生 SEGFAULT。现在两者都改为：

```text
fixed semantic count = 256
physical storage = heap-backed std::vector(...256)
```

仍保留 `cells[i] / range-for / size()` 语义，但不再依赖扩大线程栈。

## CLI

### Source-only Probe（P0 首选）

CMake target：`turtle335_probe_adt`

```text
turtle335_probe_adt <source.adt> [--wdt <source.wdt>]
```

不需要 `LiquidType.dbc`，不生成目标文件。统计：

```text
MCAL: none / legacy4 / big8 / RLE8 / mixed / unknown
MCSH / MCCV / MCSE / legacy-MCLQ
MCSE emitter count
MH2O raw LiquidType IDs + layer/cell count
MFBO
high-res holes
source +0x3E
disable_doodads_map
source tail dwords
unverified MCNK flags
invalid M2/WMO refs
WDT big-alpha mismatch
```

exit：0 clean / 2 blocker / 3 risk。

### Full scan/convert

CMake target：`turtle335_convert_adt`

扫描：

```text
turtle335_convert_adt <source.adt> <LiquidType.dbc> [--wdt <source.wdt>] --scan-only
```

exit：0 lossless / 2 blocker / 3 lossy。

转换：

```text
turtle335_convert_adt <source.adt> <LiquidType.dbc> <output.adt> [--wdt <source.wdt>]
```

默认 fail-closed；Loss需显式 `--allow-lossy`；先target validator再atomic new-file write，不覆盖源/已有输出。

## Tests

当前 CTest 共 15 项，包括：

```text
test_normalized_adt.cpp
test_wotlk_adt_reader.cpp
test_wotlk_adt_probe.cpp
test_wotlk_terrain_normalizer.cpp
test_wotlk_mcsh_normalizer.cpp
test_wotlk_adt_normalizer.cpp
test_liquid_type_dbc.cpp
test_wotlk_wdt_reader.cpp
test_raw_wotlk_pipeline.cpp
```

`test_raw_wotlk_pipeline`真实在完整ADT bytes中 insert O2HM、patch MHDR、shift 256 MCIN offsets，然后 Parse->Normalize->Serialize->Validate->reparse target；reparse 同时锁定 `sizeMCLQ=812` / inner size=0 的 owning-block行为。

Probe 测试覆盖 old4/big8/RLE、MCSH/MCCV/MCSE、high-res holes、source字段、MH2O LiquidType ID、WDT mismatch，以及 `QLCM inner size=0 + 812B owning block`。

## 最新跨平台验证状态 — 已恢复绿色

通过临时 draft PR 只添加 CI marker，利用现有 `pull_request` workflow 对当前 main code 做真实 Ubuntu/Windows 验证；所有 marker PR 均已关闭、未合并。

最终验证：

```text
workflow: core-tests
run id: 32095429158
run number: 97
validated main code SHA: 66920cb17dda11970e9bbe6cc9e6c45ecc76d7d7
```

结果：

```text
Ubuntu:
  Configure PASS
  Build PASS
  CTest PASS (15/15)
  Fixture A generation PASS
  disk validator PASS
  SHA record PASS
  artifact upload PASS

Windows:
  Configure PASS
  Build PASS
  CTest PASS (15/15)
  Windows tools artifact upload PASS
```

本轮 CI 连续发现并修掉三类真实问题：

1. Validator残留过时 bit15=0 规则；
2. Reader把 inner-size=0 的完整 legacy MCLQ 错截成8B；
3. Normalized/Writer大型256-cell内嵌数组导致 Windows stack SEGFAULT。

因此当前 main **不再是“仅静态审查”状态**，而是最新代码已完成 Ubuntu+Windows CI 编译与测试验证。

## 公开真实数据线索

### build12340 LiquidType.dbc

公开仓库：`DreamCoreRev/EonsDBC`，说明为 3.3.5a.12340 DBC。实际 `LiquidType.dbc` WDBC header 已读取：

```text
recordCount = 26
fieldCount = 45
recordSize = 180
stringBlockSize = 720
```

与当前严格 WDBC parser 的 `recordSize == fieldCount*4` 约束一致。

### Noggit ADT fixture lead

`wowdev/noggit3` issue #87 提供 `ADT.zip` 公开附件，可作为公开、可追溯编辑器/ADT fixture 线索；它不是未经修改的 Blizzard stock tile，因此只能用于格式回归，不能替代最终真实客户端地图验证。当前工具连接限制下尚未取得附件二进制。

## 当前 Loss / Blocker

Loss/Risk：

```text
MCCV
MCSE（尚未完成跨版本 emitter 语义验证）
MFBO
source +0x3E
disable_doodads_map
unverified tail dwords
unverified MCNK flags
```

Blocker：

```text
high-resolution holes
invalid MCRF refs
unknown/missing LiquidType
legacy source MCLQ without semantic importer
unsupported/mixed MCAL encoding
global-WMO WDT in terrain-only profile
```

## 下一步 P0

不要继续扩展 speculative chunks。下一步必须优先真实 fixture：

1. 获得真实 build12340 `.adt`；首先跑 `turtle335_probe_adt`，即使暂时没有DBC也可完成 source feature inventory；
2. 有对应 `.wdt` 时同时提供，核对 MPHD big-alpha；
3. 再用 build12340 `LiquidType.dbc` 跑 `turtle335_convert_adt --scan-only`；
4. 统计真实 Loss/Blocker；
5. 选择最简单真实 tile 转换；
6. target validator重开；
7. Noggit打开/保存/重开；
8. Turtle 1.18.1 Local Files 实机加载；
9. 检查 terrain seam / MCAL / MCSH / holes / placement / liquid。

真实 fixture通过后，再决定补真实需要的MCSE/MFBO，或冻结ADT基线并转入 M2 v264->v256。

## 当前专题入口

```text
docs/research/ADT_IMPLEMENTATION_CHECKPOINT_2026-08-18.md
docs/research/ADT_WOTLK_NORMALIZATION_CHECKPOINT_2026-08-18.md
docs/research/NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md
docs/research/ADT_MCAL_BINARY_ANALYSIS.md
docs/research/WOW_CRUCIBLE_PLACEMENT_AUDIT_2026-08-18.md
docs/research/M2_RETROPORT_SPEC.md
docs/research/WMO_RETROPORT_SPEC.md
```
