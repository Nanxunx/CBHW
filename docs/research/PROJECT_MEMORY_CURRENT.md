# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 15:05 +08:00

本文件是 `NansenCore/Turtle335Converter` 的当前记忆入口。

## 当前扫描策略：V4.3 Targeted

用户指出 V4.2 对约11597 pair 全量深扫描存在明显冗余。该判断成立。

V4.2 保留为“全库 production gate”工具，但当前日常研究入口切换为 V4.3 两阶段定向扫描：

```text
tools/modelport/modelport_targeted_scan_v43.py
tools/modelport/Run_ModelPort_TargetedScan_V43.ps1
```

V4.3：

```text
Phase 1：全库只读取约324B M2 Header
 -> 识别 Animation / Ribbon / Particle / TexAnim / Event / ExternalAnim
 -> 检查 335 v264 / target v256 与基本 count 异常

Phase 2：只对当前真正相关的模型做深度二进制比较
 -> animations > 0
 -> ribbons > 0
 -> particles > 0
 -> texanims > 0
 -> events > 0
 -> external .anim
 -> version/count anomaly
```

普通静态模型不再运行 Sequence / AnimationLookup / 226 Playable 深度比较。

自动优先打包：

```text
1. Rule mismatch
2. true RibbonEmitter > 0
3. Particle + one animation
4. complex Particle
5. Alias / SubAnimation
```

## V4.2 状态

V4.2 checkpoint/resume 仍保留，用于未来需要“一次性全库证明 Playable production baseline”时运行：

```text
tools/modelport/modelport_fullscan_v42_resume.py
tools/modelport/Run_ModelPort_FullScan_V42_RESUME.ps1
```

但当前无需继续等待 V4.2 扫描完整11597对。

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

Ribbon 仍缺真正 `nRibbonEmitters > 0` 的 Golden Sample，V4.3 必须按 Header 自动筛选，禁止按文件名猜。

## Golden Source

两个目录保持只读：

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
