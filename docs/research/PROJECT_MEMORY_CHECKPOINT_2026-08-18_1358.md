# 模型移植项目 — 最新记忆检查点（2026-08-18 13:58 +08:00）

本文件记录 `NansenCore/Turtle335Converter` 在本轮长对话结束前的最新工程状态。它是 `PROJECT_MEMORY_CURRENT.md` 的**增量权威检查点**：若本文件与 `PROJECT_MEMORY_CURRENT.md` 在以下更新项上冲突，以本文件为准；未提及的长期 M2/WMO/ADT 结论继续沿用 `PROJECT_MEMORY_CURRENT.md`。

## 用户持续指令

- 本项目属于长期“模型移植”项目。
- 用户说“继续”时直接推进，不重复询问已确定目标。
- 用户偏好源码/二进制/真实 fixture 证据，避免未经验证的格式数字。
- 当前最高优先级仍是**真实 WoW 3.3.5a build12340 ADT/WDT fixture 验证**，而不是继续扩展 speculative chunk。

## 当前转换目标

```text
WoW 3.3.5a build 12340
 -> semantic retroport
Vanilla 1.12.x / Turtle WoW 1.18.1 build 7272
```

核心原则仍是：

```text
Reader
 -> Normalized semantic model
 -> semantic downgrade
 -> target Writer
 -> validator
 -> real-client/server regression
```

禁止只改版本号、跨世代 raw memcpy、同 offset=同语义、删块后继续沿用旧 offsets。

## 本轮最重要的新工程结论

### 1. bit15 Validator 旧规则已彻底修正

旧 Validator 曾错误要求：

```text
canonical old-MCLQ target -> bit15 must be 0
```

真实客户端二进制分析后的当前规则：

```text
bit15=1 -> 客户端使用完整 64x64 alpha/shadow 边缘
bit15=0 -> 客户端从 row/col 62 补 row/col 63
```

因此：

- 结构 Validator 允许 bit15=0/1 两种合法解码模式；
- production Normalizer/Writer 统一物化完整边缘并设置 bit15=1；
- `test_normalized_adt` 单独锁定 production bit15=1；
- 任何旧资料中“target 必须 bit15=0”的结论均已废弃。

### 2. legacy MCLQ owning-size 规则已写入 Reader / Normalizer / Probe

重要目标布局：

```text
QLCM FourCC
inner size = 0  // 合法
真实总块长度 = MCNK.sizeMCLQ
sizeMCLQ = 8 + 804 * categoryRecordCount
```

之前 `WotlkAdtReader` 使用通用 `innerSize + 8`，会把一个 `812B` MCLQ 错截为 8B。现已修正为：

```text
legacy QLCM -> 使用 MCNK.sizeMCLQ 拷贝 owning block
```

`WotlkAdtNormalizer` 和 `WotlkAdtProbe` 同步采用这条规则，不再把 inner-size=0 误判为损坏。

### 3. Windows 默认栈溢出已从根因修复

GitHub Windows runner 曾出现：

```text
normalized_adt_tests -> SEGFAULT
wotlk_adt_normalizer_tests -> SEGFAULT
raw_wotlk_pipeline_tests -> SEGFAULT
```

根因不是格式差异，而是大型对象内嵌 256 cells：

```text
NormalizedAdt::cells
AdtWriterInput::cells
```

在 Windows 默认约 1MiB stack 上容易溢出。

最终修法不是扩大 `/STACK`，而是改为：

```text
semantic count = 256
physical storage = heap-backed std::vector(..., 256)
```

仍保持：

```text
cells[i]
range-for
size()
```

真实 Windows CLI 与测试因此一起安全。

## Source-only Probe 已完成

CMake target：

```text
turtle335_probe_adt
```

调用：

```text
turtle335_probe_adt <source.adt> [--wdt <source.wdt>]
```

特点：

- 不需要 `LiquidType.dbc`；
- 不写 target 文件；
- 适合作为任何真实 3.3.5a ADT 的第一层审计。

统计内容：

```text
MCAL none / legacy4 / big8 / RLE8 / mixed / unknown
MCSH
MCCV
MCSE + emitter count
legacy MCLQ
MH2O raw LiquidType IDs + layer/cell count
MFBO
high-resolution holes
source +0x3E
disable_doodads_map
source tail dwords
unverified MCNK flags
invalid M2/WMO placement refs
WDT big-alpha mismatch
```

退出码：

```text
0 = clean
2 = blocker
3 = risk/loss without blocker
```

## 批量地图 Probe 已完成

新模块/工具：

```text
WotlkMapProbe.h
WotlkMapProbe.cpp
tools/probe_wotlk_map.cpp
tests/test_wotlk_map_probe.cpp
```

CMake target：

```text
turtle335_probe_map
```

调用：

```text
turtle335_probe_map <adt-directory> [--recursive] [--wdt <source.wdt>] [--details]
```

用途：一次扫描整个地图目录内所有 `.adt`，将 tile 分类为：

```text
Clean
Risk
Blocker
ParseFailure
```

并汇总：

```text
issue code -> occurrence count + affected tile count
MH2O LiquidType ID -> layer count + affected tile count
MCAL encoding distribution
MCCV / MCSE / MFBO usage
high-res holes / source field usage
clean conversion candidates
```

默认输出摘要和 clean candidate；`--details` 才展开逐 tile，避免大型地图数千文件时日志失控。

批量工具退出码：

```text
0 = all scanned tiles clean
2 = at least one blocker or parse failure
3 = no blocker but at least one risk/loss
```

## 当前测试数量

批量 Probe 加入后：

```text
CTest = 16 tests
```

新增聚合测试覆盖：

```text
clean/risk/blocker/parse-failure 分类
issue code tile 去重统计
LiquidType 跨 tile 汇总
```

## 最新跨平台 CI — 当前主线已验证

最新批量 Probe 验证：

```text
workflow: core-tests
run id: 32095952833
run number: 104
validated main code SHA: 771cf518d4a49bf88c07ef752721f918a65eb14d
```

结果：

```text
Ubuntu:
  Configure PASS
  Build PASS
  CTest PASS (16/16)
  Fixture A generation PASS
  disk validator PASS
  SHA record PASS
  artifact upload PASS

Windows:
  Configure PASS
  Build PASS
  CTest PASS (16/16)
  Windows tools artifact upload PASS
```

因此当前 ADT Reader/Normalizer/Writer/Validator/Probe/Batch Probe 不是“仅静态审查”，而是已经通过真实 Ubuntu + Windows Actions 编译与测试。

## 本轮 CI 演进历史（用于避免重复排查）

通过临时 draft PR 仅添加 marker 触发 `pull_request` workflow，所有 marker PR 均不应合并。

连续发现并修复：

```text
第一类：Validator 过时 bit15=0 强制规则
第二类：legacy QLCM inner-size=0 被 Reader 截成 8B
第三类：Normalized/Writer 256-cell 大型内嵌数组导致 Windows stack SEGFAULT
第四类：Probe 自身仍使用通用 QLCM inner-size 检查，与 Reader 不一致
```

最终全部修复并双平台全绿。

## 当前公开真实 fixture 线索

### build12340 LiquidType.dbc

公开仓库：

```text
DreamCoreRev/EonsDBC
```

已实际读取 `LiquidType.dbc` WDBC header：

```text
recordCount = 26
fieldCount = 45
recordSize = 180
stringBlockSize = 720
```

与当前 Parser 的：

```text
recordSize == fieldCount * 4
```

约束一致。

### Noggit ADT fixture lead

`wowdev/noggit3` issue #87 有公开 `ADT.zip` 附件，可用于公开可追溯的 ADT 编辑器/格式回归，但不是未经修改的 Blizzard stock tile，因此不能替代最终真实客户端 fixture。

当前工具下载桥对旧 issue attachment 有限制，尚未把该附件纳入仓库。

## 当前真实 P0 缺口

当前最大的缺口已经不是编译或 synthetic fixture，而是：

```text
真实、未经修改的 WoW 3.3.5a build12340 ADT
对应 WDT（最好有）
对应 build12340 LiquidType.dbc
```

当前对话/File Library 已搜索过，没有找到可直接使用的真实 ADT/WDT fixture。

## 下一步执行顺序

如果用户继续推进，不要询问重复问题，按以下顺序执行：

1. 如果用户上传真实 3.3.5a 地图目录或 ADT：
   - 先 `turtle335_probe_map`（目录）或 `turtle335_probe_adt`（单 tile）；
   - 不需要先等 LiquidType.dbc。
2. 统计真实：
   - MCAL encoding 分布；
   - MH2O LiquidType IDs；
   - MCCV/MCSE/MFBO/high-res holes/legacy MCLQ 等实际使用率；
   - blocker/risk 最常见 issue code。
3. 自动选最简单 `Clean` 或最低风险 tile 作为第一真实 conversion fixture。
4. 有对应 `LiquidType.dbc` 后跑：

```text
turtle335_convert_adt <adt> <LiquidType.dbc> --wdt <wdt> --scan-only
```

5. 通过 semantic scan 后生成 target ADT。
6. 依次验证：

```text
target AdtValidator
Noggit open/save/reopen
Turtle 1.18.1 Local Files 实机加载
terrain seams
MCAL layers
MCSH shadow edges
holes
M2/WMO placements
liquid surface/type/deep/fishable
```

7. 真实 fixture 通过后：
   - 如果真实数据大量使用 MCSE/MFBO，再按实际需求补；
   - 否则冻结 ADT baseline，转入 M2 v264 -> target v256 实现。

## M2 下一阶段保持不变

已确认目标：

```text
WotLK MD20 = 264
Turtle accepts = 256/257
production target = 256
```

关键结构：

```text
Skin 48B -> embedded View 44B
Submesh 48 -> 32
Animation 64 -> 68
Bone 88 -> 108
Ribbon 176 -> 220
Particle 476 -> 504
Vertex 48 -> 48
```

M2 实现必须做 external SKIN -> embedded View、legacy tracks、quaternion conversion、Ribbon/Particle downgrade，不能只改 version。

## 当前仓库关键状态

截至本检查点：

```text
repo: NansenCore/Turtle335Converter
branch: main
latest batch-probe validated code SHA: 771cf518d4a49bf88c07ef752721f918a65eb14d
README later updated with current CI/batch-probe status
```

本文件保存后，若开启新对话，优先读取：

```text
docs/research/PROJECT_MEMORY_CURRENT.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
README.md
```

然后直接从“真实 build12340 fixture 批量 Probe”继续。
