# 模型移植项目 — 当前权威记忆

更新时间：2026-08-18

本文件是 `NansenCore/Turtle335Converter` 当前优先读取的项目记忆。旧的 `PROJECT_MEMORY_2026-08-18.md` 保留为历史记录；若两者冲突，以本文件、最新专题文档、真实客户端二进制和当前源码为准。

## 当前用户指令

- 本项目属于“模型移植”长期项目，后续相关对话继续沿用本文件中的证据、架构和实现进度。
- 用户要求“继续”时直接推进，不重复询问已经确定的目标和基础约束。
- 当前最高优先级仍是**真实 WoW 3.3.5a build12340 fixture 验证**；在没有真实 `ADT/WDT/LiquidType.dbc` 输入时，不把时间耗在 speculative chunk 上，应优先完善真实 fixture 扫描/诊断/批处理能力和编译验证入口。
- 2026-08-18 本轮已搜索当前会话上传与 File Library，没有找到可直接作为真实 build12340 `ADT/WDT/LiquidType.dbc` fixture 的文件。因此继续工程推进，但仍把真实 fixture 保留为 P0 验证门槛。

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

每类 804B record，顺序 River->Ocean->Magma->Slime；完整区域 `8 + 804*N`，QLCM inner size=0。

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

因此 source raw bit15 不 passthrough；Normalizer先物化完整边缘，production target `fullAlphaShadowEdges=true`，Writer显式设置 target bit15。任何旧文档中“canonical target总是bit15=0”的描述均已过时。

## MCSH

Noggit确认 payload=512B=64*uint64；source bit15=0 时 col63<-62、row63<-62、corner<-62,62。`WotlkMcshNormalizer` 已实现；全零 shadow 自动省略。Target 使用完整边缘+bit15=1。

## MCNR

Noggit保存模型：MCNR declared payload=435B (145*3)，随后有13B extra bytes在 chunk 外，再开始MCLY。当前 Reader/Writer按此实现。

## MCCV

Production 目前**不支持直接保留**。Turtle MCNK pointer-fixup `0x6AF970`处理 MCVT/MCNR/MCLY/MCRF/MCAL/MCSH/MCSE/MCLQ，但没有建立 `offsMCCV (+0x74)` runtime pointer。`NormalizedAdt` 已移除 `targetMccv` production field。Source MCCV继续报告 Loss。

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

Reader解析 root/path catalogs/placements/256 MCNK/MCRF/raw terrain chunks/MH2O/MFBO。

`LiquidTypeDbc`按 Trinity build12340 规则：field0=ID，field3=SoundBank；0 Water/1 Ocean/2 Magma/3 Slime。

`WotlkWdtReader`解析 MVER18、MPHD、MAIN；提供 globalWmo 与 bigAlpha；terrain-only target保留tile presence并清 source feature flags。

## CLI

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

最新新增测试：

```text
test_normalized_adt.cpp
test_wotlk_adt_reader.cpp
test_wotlk_terrain_normalizer.cpp
test_wotlk_mcsh_normalizer.cpp
test_wotlk_adt_normalizer.cpp
test_liquid_type_dbc.cpp
test_wotlk_wdt_reader.cpp
test_raw_wotlk_pipeline.cpp
```

`test_raw_wotlk_pipeline`真实在完整ADT bytes中 insert O2HM、patch MHDR、shift 256 MCIN offsets，然后 Parse->Normalize->Serialize->Validate->reparse target。

## 最新验证状态

历史较早的 Writer/FixtureA revision 曾在 Ubuntu/Windows Actions 通过；**最新 Reader/Normalizer/DBC/WDT/MCSH/CLI/raw-pipeline 提交在当前连接器会话中尚未获得新的可见 CI run，也无法拉取私有repo archive到本地重编译。**

因此必须描述为：代码已提交、测试已提交、静态审查已做；最新 main 尚未重新 CI/本地 build 验证。

## 当前 Loss / Blocker

Loss：

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

1. 真实 build12340 ADT + 对应WDT + LiquidType.dbc 跑 `--scan-only`；
2. 统计真实 Loss/Blocker；
3. 选择最简单真实 tile 转换；
4. target validator重开；
5. Noggit打开/保存/重开；
6. Turtle 1.18.1 Local Files 实机加载；
7. 检查 terrain seam / MCAL / MCSH / holes / placement / liquid。

fixture通过后，再决定补真实需要的MCSE/MFBO，或冻结ADT基线并转入 M2 v264->v256。

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
