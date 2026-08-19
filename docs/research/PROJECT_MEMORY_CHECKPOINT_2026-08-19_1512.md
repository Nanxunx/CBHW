# 模型移植项目检查点 — 2026-08-19 15:12 +08:00

## 扫描策略调整：V4.4 Focused Targeted Scan

用户指出：当前阶段没有必要把约 11,597 对同路径 M2 全部执行深度 Sequence / AnimationLookup / 226 Playable 比较。该判断成立。

V4.2 全库深扫描保留为最终 production gate 工具，但不再作为当前日常研究入口。

## V4.4 三阶段

```text
Phase 1
全库只读取 M2 Header（约324B）
-> Animation / Ribbon / Particle / TexAnim / Event / count/version 分类

Phase 2
只对有 Animation 的 335 source 读取 64B Sequence records
-> Alias(flags&0x40)
-> SubAnimationID
-> duplicate AnimationID
-> MaxAnimationID

Phase 3
只对真正高风险/未解决 Feature 做 335↔成功112 深度二进制比较：
-> RibbonEmitter > 0
-> Particle > 0
-> TexAnim > 0
-> external .anim
-> Alias
-> SubAnimation
-> duplicate AnimationID
-> source/target version/count anomaly
```

普通动画模型额外只抽样 24 个高风险候选做回归；普通静态模型不再深扫。

## 转换队列

V4.4 自动生成 `ConversionQueues/`：

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

这些队列将直接作为后续 batch converter 的 feature gate 输入。

## 当前研究优先级

1. 真正 `nRibbonEmitters > 0` Golden Samples
2. Particle one-animation 特殊分支
3. Particle complex / alias / subanimation
4. Animation V4 只做定向回归
5. 已验证 static geometry 不再重复深扫

## 当前有效动画基础

保持 Golden V4：

```text
Sequence.Index = preserve source
AnimationLookup count = max(AnimationID)+1
duplicate AnimationID -> prefer SubAnimationID 0
PlayableAnimationLookup = 226
quaternion -1 -> exact +1.0
3333ms Classic timeline
skin -> embedded View
```

第二批 13/13 成功 target 的全部 226 Playable records 已 exact match；V4.1 额外 6 个第三批假阳性 pair 也已回归通过。

## 权威仓库

```text
NansenCore/Turtle335Converter
branch: main
```

当前扫描器：

```text
tools/modelport/modelport_targeted_scan_v44.py
tools/modelport/Run_ModelPort_TargetedScan_V44.ps1
```
