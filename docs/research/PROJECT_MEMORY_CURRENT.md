# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 15:12 +08:00

本文件是 `NansenCore/Turtle335Converter` 的当前记忆入口。

## 当前扫描策略：V4.4 Focused Targeted

用户指出 V4.2 对约11597 pair 全量深扫描存在明显冗余。该判断成立。

V4.2 保留为“最终全库 production gate”工具，但当前日常研究入口切换为 V4.4 三阶段定向扫描：

```text
tools/modelport/modelport_targeted_scan_v44.py
tools/modelport/Run_ModelPort_TargetedScan_V44.ps1
```

V4.4：

```text
Phase 1：全库只读约324B M2 Header
 -> Animation / Ribbon / Particle / TexAnim / Event / version/count 分类

Phase 2：只对有 Animation 的 335 source 读取 64B Sequence records
 -> Alias(flags&0x40)
 -> SubAnimationID
 -> duplicate AnimationID
 -> MaxAnimationID

Phase 3：只对当前真正相关的高风险 Feature 做 335↔成功112 深度比较
 -> RibbonEmitter > 0
 -> Particle > 0
 -> TexAnim > 0
 -> external .anim
 -> Alias
 -> SubAnimation
 -> duplicate AnimationID
 -> source/target version/count anomaly
```

普通动画模型额外只抽样24个高风险候选回归；普通静态模型不做 Sequence / AnimationLookup / 226 Playable 深度比较。

## V4.4 转换队列

自动生成 `STAGING/00_Metadata/ConversionQueues/`：

```text
STATIC_GEOMETRY_SAFE
ANIMATION_BASELINE
ANIMATION_HIGH_RISK_GOLDEN
EXTERNAL_ANIM_COPY
TEXANIM_GOLDEN_VALIDATED
NEEDS_PARTICLE_FULL_WRITER
NEEDS_RIBBON_FULL_WRITER
NO_GOLDEN_PAIR
BLOCK_HEADER_ERROR
```

这些队列直接作为后续 batch converter 的 feature gate 输入。

## V4.2 状态

V4.2 checkpoint/resume 仍保留，用于未来需要“一次性全库证明 Playable production baseline”时运行：

```text
tools/modelport/modelport_fullscan_v42_resume.py
tools/modelport/Run_ModelPort_FullScan_V42_RESUME.ps1
```

当前无需继续等待 V4.2 扫描完整11597对。

## V4.1/V4 已确认基础

第三批原 PowerShell 全库扫描的 `scan_errors=11597` 是扫描器故障，不是模型规则失败。自动挑出的6个 mismatch 经 V4.1 独立解析全部是假阳性。

V4.1 已在第三批6个假阳性 pair + 第二批13个 Golden pair，共19个 pair 回归通过：

```text
scan_errors = 0
Sequence mismatch = 0
Timeline mismatch = 0
AnimationLookup mismatch = 0
Playable mismatch = 0
```

第二批 13/13 成功 target 的 226 Playable records 已全部 exact match。

## 当前 Golden V4 转换规则

```text
MD20 v264 -> standard MD20 v256
Sequence.Index = preserve source
AnimationLookup = max(AnimationID)+1
duplicate ID -> prefer SubAnimationID 0
Playable = 226
quaternion -1 -> exact +1.0
skin -> embedded View
```

已验证：

```text
embedded View: 17/17
普通 3D BLP: 39/39 SHA exact
external .anim: 2/2 SHA exact
TextureAnimation: 10 records
Particle: 50 emitters（部分规则已确认，FULL writer尚未BATCH_READY）
```

Ribbon 仍缺真正 `nRibbonEmitters > 0` 的 Golden Sample。V4.4 必须按 Header 自动筛选，禁止按文件名猜。

## Golden Source

两个目录保持只读：

```text
E:\335_FinalExtract_V5
E:\335to112_Converted_FinalExtract_V1
```

## 最新检查点

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1512.md
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
