# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 07:02 +08:00

本文件是 `NansenCore/Turtle335Converter` 的当前记忆入口。

## 最新 M2 / ModelPort 检查点

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0702.md
docs/research/M2_THIRD_BATCH_SCAN_RECOVERY_V41_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
```

## 当前最重要状态

第三批原 PowerShell 全库扫描：

```text
paired = 11597
scan_errors = 11597
```

因此该次输出的全库 mismatch=0、Ribbon=0、Particle=0 等统计无效。

自动打包的 6 个所谓 V4 mismatch 已独立重新解析，全部是假阳性：

```text
6/6 Sequence exact
6/6 AnimationLookup exact
6/6 Playable exact
```

已经切换到：

```text
tools/modelport/modelport_fullscan_v41.py
tools/modelport/Run_ModelPort_FullScan_V41.ps1
```

V4.1 已在：

```text
第三批6个假阳性 pair
+
第二批13个 Golden pair
```

共 19 个 pair 上回归：

```text
scan_errors = 0
Sequence mismatch = 0
Timeline mismatch = 0
AnimationLookup mismatch = 0
Playable mismatch = 0
```

第二批 feature 统计也正确恢复为：

```text
Particle models = 7
TexAnim models = 3
External anim models = 2
Alias models = 3
SubAnimation models = 2
Ribbon models = 0
```

## Production gate

Playable V4 第二批 13/13 exact 仍有效，但尚不能宣告整个 11597-pair 库 production-ready。

必须先满足：

```text
V4.1 full scan:
scan_errors == 0
```

再检查：

```text
Playable V4 mismatch == 0
```

## 转换规则

保持 Golden V4：

```text
MD20 v264 -> standard MD20 v256
Sequence.Index = preserve source
AnimationLookup = max(AnimationID)+1
duplicate ID -> prefer SubAnimationID 0
Playable = 226
quaternion -1 -> exact +1.0
skin -> embedded View
```

Golden Source 两个目录都只读：

```text
E:\335_FinalExtract_V5
E:\335to112_Converted_FinalExtract_V1
```

## ADT

ADT 详细记忆仍从：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
```

读取。

## 权威仓库

```text
NansenCore/Turtle335Converter
branch: main
```

`WYTurtle` 不是模型转换器源码仓库。
