# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 06:23 +08:00

本文件是 `NansenCore/Turtle335Converter` 的当前记忆入口。详细历史不再重复堆叠在一个文件里；按专题读取最新检查点。

## 最新检查点

### M2 / ModelPort（当前最高相关）

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0623.md
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
```

### ADT / 地图

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
```

如果旧文档与最新检查点冲突，以：

```text
真实目标客户端/成功目标二进制
> 最新 Golden Reference
> 当前源码与测试
> 历史工具/社区文档
```

为证据优先级。

## 权威仓库

```text
repo: NansenCore/Turtle335Converter
branch: main
```

`WYTurtle` 不是本模型转换器源码仓库；后续转换器源码、研究文档、测试、记忆只同步到 `NansenCore/Turtle335Converter`。

## 目标

```text
WoW 3.3.5a build12340
 -> semantic retroport
Vanilla 1.12.x / Turtle WoW 1.18.1
```

核心架构：

```text
Reader
 -> normalized semantic model
 -> semantic downgrade
 -> target writer
 -> validator
 -> real-client regression
```

禁止：只改 version、跨世代 raw memcpy、删除块后保留旧 offset、把历史工具私有格式作为输出。

最终 M2 输出只允许标准 `MD20 v256`；OrangeM2 等只作为算法参考，不能成为存储格式或运行依赖。

## 当前本地 Golden 数据基线

```text
335 Raw Complete:
E:\335_FinalExtract_V5

成功降版 1.12 Golden:
E:\335to112_Converted_FinalExtract_V1
```

成功参考 M2 约 12,512；已知同路径 335↔112 M2 pairs 约 11,597。

## M2 当前关键结论（Golden V4）

目标结构：

```text
WotLK MD20 v264 -> Classic/Turtle MD20 v256
Animation 64B -> 68B
Bone 88B -> 108B
Skin 48B submesh -> Classic 32B submesh embedded View
Particle 476B -> 504B
```

### Sequence

```text
Sequence.Index = preserve source value
```

不得写0，也不得重建成 physical array position。

Sequence timeline：每条前 `+3333ms`，`end=start+source.length`。

### AnimationLookup

```text
count = max(AnimationID)+1
missing = 0xFFFF
```

重复 AnimationID：优先 `SubAnimationID==0`；没有 sub0 才取第一 physical occurrence。

### PlayableAnimationLookup

固定 `226 x 4B`。

当前 V4 采用 Golden-validated fallback graph，而不是直接读取 build12340 AnimationData.dbc fallback 字段。

第二批新增 Golden extension：

```text
170->16 171->16 172->16 173->16 174->16
175->30 176->16 178->16 179->16 181->16
191->159
```

第二批 13/13 成功 target 的全部226项逐项 exact match。

### 已验证稳定部分

- quaternion compressed `-1 -> +1.0f` exact
- generic Legacy Range/Times/Keys
- external `.skin -> embedded View`，第二批17 views PASS
- ordinary compatible 3D BLP 原样保留，第二批39/39 SHA exact
- FelReaver/Muru external `.anim` 原样复制，2/2 SHA exact
- TextureAnimation 可复用统一 legacy track serializer

### Particle

第二批已验证 50 emitters：

```text
source stride=476
target stride=504
target flags = source flags & 0xFFFF
```

Fake gradient 3-key 降级已有强证据，但单 Sequence particle track 仍有特殊分支，FULL Particle writer 尚未 BATCH_READY。

### Ribbon

第二批没有真正 `nRibbonEmitters>0` 样本；文件名不能作为判断。第三批必须从全库 Header 自动选择 true Ribbon。

## 当前验证策略

不再一把模型 V1/V2/V3 反复进客户端。

```text
全库 Feature/Metadata Scan
 -> Golden class validation
 -> offline validator
 -> 每个 feature class 只做 1~2 个 real-client Golden test
 -> 批量解锁
```

第三批脚本：

```text
Pack_ModelPort_GoldenReference_ThirdBatch_AutoScan.ps1
```

全库验证 Sequence / AnimationLookup / Playable，并自动选 V4 mismatch、true Ribbon、Particle one-animation/complex、Alias/SubAnimation 样本。

如果全库 Playable V4 mismatch=0，则将 Playable graph 从 provisional 升级为 production baseline。

## ADT 记忆入口

ADT 仍以：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
```

为详细权威检查点；其中包括 legacy MCLQ owning-size、MCAL/bit15、MCSH、Windows heap-backed 256-cell、批量 Probe 和跨平台 CI 等结论。
