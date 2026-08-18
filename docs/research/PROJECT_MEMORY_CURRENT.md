# 模型移植项目 — 当前权威记忆

更新时间：2026-08-18 13:58 +08:00

> **最新状态入口：** 本项目本轮长对话的最新增量状态已保存到 `docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md`。凡涉及 **批量 ADT Probe、16/16 跨平台 CI、legacy MCLQ owning-size、Windows heap-backed 256-cell 修复、最新 P0 顺序** 的内容，以该检查点为准。

本文件保留长期稳定结论；若旧段落与最新检查点冲突，以最新检查点、当前源码、真实客户端二进制和真实 fixture 为准。

## 当前用户指令

- 本项目属于“模型移植”长期项目，后续相关对话继续沿用项目记忆文件中的证据、架构和实现进度。
- 用户要求“继续”时直接推进，不重复询问已经确定的目标和基础约束。
- 当前最高优先级仍是**真实 WoW 3.3.5a build12340 fixture 验证**。
- 没有真实 `ADT/WDT/LiquidType.dbc` 时，不把时间耗在 speculative chunk；优先完善真实 fixture 扫描、诊断、批处理和验证能力。

## 目标

```text
WoW 3.3.5a build 12340
 -> semantic retroport
Vanilla 1.12.x / Turtle WoW 1.18.1 build 7272
```

原则：

```text
Reader
 -> Normalized semantic model
 -> semantic downgrade
 -> target Writer
 -> validator
 -> real-client/server regression
```

禁止只改版本号、跨世代 raw memcpy、同 offset=同语义、删除块后沿用旧 offset。

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

M2 必须做 external SKIN->embedded View、legacy track rebuild、quaternion conversion、Ribbon/Particle downgrade；不能只改 version。

主要算法参考 Koward/M2Lib；M2Workshop 用于 repair/regression；warcraft-rs/wow-m2 主要用作 parser/struct reference。

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

每类 804B record，顺序 River->Ocean->Magma->Slime；完整区域：

```text
8 + 804 * N
```

重要：legacy `QLCM` inner chunk size 可以是 0，真实总长度由 `MCNK.sizeMCLQ` 持有。Reader/Normalizer/Probe 必须使用 owning MCNK size 识别完整块。

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

## MCAL + bit15 最终规则

Source：

```text
MCLY 0x100 USE_ALPHA
MCLY 0x200 COMPRESSED
2048B old packed4
4096B big8
compressed RLE8
WDT MPHD&0x4 = big-alpha hint
```

Normalizer 先还原完整 64x64，再将 old sequential alpha 转成 independent contributions；mixed old4+big/RLE 暂时拒绝。

Target：

```text
bit15=1 -> Turtle使用完整64x64 alpha/shadow边缘
bit15=0 -> Turtle从62复制生成row63/col63
```

因此 source raw bit15 不 passthrough；Normalizer先物化完整边缘，production target `fullAlphaShadowEdges=true`，Writer显式设置 target bit15。

结构 Validator 允许 bit15=0/1 两种合法解码模式；production 策略由测试单独锁定为 bit15=1。

## MCSH

Noggit确认 payload=512B=64*uint64；source bit15=0 时 col63<-62、row63<-62、corner<-62,62。`WotlkMcshNormalizer` 已实现；全零 shadow 自动省略。Target 使用完整边缘+bit15=1。

## MCNR

Noggit保存模型：MCNR declared payload=435B (145*3)，随后有13B extra bytes在 chunk 外，再开始 MCLY。当前 Reader/Writer 按此实现。

## MCCV

Production 目前不直接保留。Turtle MCNK pointer-fixup `0x6AF970` 处理 MCVT/MCNR/MCLY/MCRF/MCAL/MCSH/MCSE/MCLQ，但没有建立 `offsMCCV (+0x74)` runtime pointer。

`NormalizedAdt` 不包含 targetMccv production field；Source MCCV 继续报告 Loss/Risk。

## Source MCNK 字段修正

Noggit build12340 source：

```text
+0x40..0x4F low_quality_texture_map[16]
+0x50..0x57 disable_doodads_map[8]
+0x78 unused1
+0x7C unused2
```

禁止把这些 raw-copy 到老 target 名称 predTex/nEffectDoodad/props/effectId。非零 source 值 -> Loss，target canonical defaults。

`+0x3C`低16位是真客户端 coarse 4x4 hole mask；`+0x3E`独立字段，非零 -> Loss，target=0。High-res holes bit16 当前 Blocker。

## 当前 ADT 实现

当前主线包含：

```text
WotlkAdtReader
WotlkAdtProbe
WotlkMapProbe
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

Reader 解析 root/path catalogs/placements/256 MCNK/MCRF/raw terrain chunks/MH2O/MFBO；legacy MCLQ 按 `MCNK.sizeMCLQ` 读取 owning block。

`LiquidTypeDbc` 按 Trinity build12340 规则：field0=ID，field3=SoundBank；0 Water / 1 Ocean / 2 Magma / 3 Slime。

`WotlkWdtReader` 解析 MVER18、MPHD、MAIN；提供 globalWmo 与 bigAlpha。

### Windows 栈安全

`NormalizedAdt` 与 `AdtWriterInput` 的 256 cell 已改成 heap-backed vector，逻辑数量仍固定为 256，避免 Windows 默认 1MiB stack SEGFAULT。

## CLI

单 ADT source-only：

```text
turtle335_probe_adt <source.adt> [--wdt <source.wdt>]
```

批量地图：

```text
turtle335_probe_map <adt-directory> [--recursive] [--wdt <source.wdt>] [--details]
```

完整 semantic scan：

```text
turtle335_convert_adt <source.adt> <LiquidType.dbc> [--wdt <source.wdt>] --scan-only
```

转换：

```text
turtle335_convert_adt <source.adt> <LiquidType.dbc> <output.adt> [--wdt <source.wdt>]
```

默认 fail-closed；Loss 需显式 `--allow-lossy`；先 target validator 再 atomic new-file write，不覆盖源/已有输出。

## 最新验证状态

最新批量 Probe 主线验证：

```text
workflow: core-tests
run id: 32095952833
run number: 104
validated main code SHA: 771cf518d4a49bf88c07ef752721f918a65eb14d
CTest count: 16
```

结果：

```text
Ubuntu: Configure PASS / Build PASS / CTest PASS 16/16
Windows: Configure PASS / Build PASS / CTest PASS 16/16
```

历史 CI 还验证并修掉：

```text
1. Validator 旧 bit15=0 强制规则
2. legacy QLCM inner-size=0 被截成8B
3. Windows 256-cell 大对象 stack SEGFAULT
4. Probe 与 Reader 的 MCLQ owning-size 规则不一致
```

当前主线已经是 Ubuntu + Windows 实际 CI 绿灯，而不是仅静态审查。

## 当前 Loss / Blocker

Loss/Risk：

```text
MCCV
MCSE（跨版本 emitter 语义尚未 production-approved）
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

不要继续扩展 speculative chunks。下一步优先真实 fixture：

1. 获得真实 build12340 ADT/地图目录；
2. 目录先跑 `turtle335_probe_map`，单 tile 跑 `turtle335_probe_adt`；
3. 统计真实 MCAL、MH2O LiquidType IDs、MCCV/MCSE/MFBO/high-res holes、source field 使用率；
4. 自动选择最简单 Clean / 最低风险 tile；
5. 有对应 `LiquidType.dbc` 后跑 `turtle335_convert_adt --scan-only`；
6. 生成 target 后跑 Validator；
7. Noggit open/save/reopen；
8. Turtle 1.18.1 Local Files 实机加载；
9. 检查 terrain seam / MCAL / MCSH / holes / placements / liquid。

真实 fixture 通过后，再决定补真实需要的 MCSE/MFBO，或者冻结 ADT baseline 转入 M2 v264 -> v256。

## 最新记忆文件

新对话优先读取：

```text
docs/research/PROJECT_MEMORY_CURRENT.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
README.md
```
