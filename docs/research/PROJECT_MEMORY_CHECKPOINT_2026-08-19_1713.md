# 模型移植项目记忆检查点 — 2026-08-19 17:13 +08:00

## 权威仓库

```text
NansenCore/Turtle335Converter
branch: main
```

这是“模型移植”项目的持久化权威记忆。后续如果聊天上下文丢失，优先读取：

```text
docs/research/PROJECT_MEMORY_CURRENT.md
本检查点
M2_GOLDEN_REFERENCE_V45_2026-08-19.md
M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
```

## 最终目标

制造实际可使用的转换工具：

```text
WoW 3.3.5a Build12340 assets
 -> semantic downgrade
 -> Vanilla 1.12.x / Turtle WoW 1.18.1 standard assets
 -> strict validator
 -> DBC merge
 -> Patch MPQ
```

最终模型输出必须是标准 `MD20 v256`。Orange/private format 只可作为算法参考，不能成为输出或客户端依赖。

## 当前 Golden corpus

```text
335 raw: <BUILD12340_SOURCE_ROOT>
successful 1.12 Golden: <CLASSIC_GOLDEN_ROOT>
```

V4.4 focused result:

```text
source M2                 18083
paired M2                 11596
deep high-risk             5036
deep errors                   0
Sequence mismatch             29
Timeline mismatch             27
AnimationLookup mismatch       0
Playable old-V4 mismatch     498
true Ribbon models           268
Particle models             5570
TexAnim models               909
external .anim               395
Alias models                 934
SubAnimation models          822
duplicate AnimID             822
```

## Production / near-production rules

### AnimationLookup — PRODUCTION_BASELINE

5036/5036 high-risk Golden pass:

```text
count = max(AnimationID)+1
missing = 0xFFFF
duplicate ID: first SubAnimationID==0, else first physical occurrence
```

`Sequence.Index` preserves source alias metadata and is independent from lookup physical position.

### PlayableAnimationLookup — V4.5 provisional

Use build12340 `AnimationData.dbc` raw field index5 recursively + current Golden overrides. Do not return to old V4 hard-coded 170->16 family.

Current V4.5 selected Golden validation is strong, but all old498 failures must be locally refined before production promotion.

### Embedded View / static geometry

- WotLK external `.skin` -> Classic embedded View is Golden-proven.
- Classic submesh = required 32-byte downgrade of WotLK 48-byte submesh.
- ordinary compatible 3D BLP is copied unchanged.
- validated external `.anim` is copied unchanged.

### Ribbon

WotLK 176B -> Classic 220B. Golden-ready offline; full-M2 integration + one real-client effect regression remains.

### Particle

WotLK 476B -> Classic 504B.

Core tracks/gradient/cells are Golden-ready. New physical correction supersedes `particle_v1.py` tail offsets for final binary output:

```text
unknown4 Vec2 @412 = zero
Rot1 @420
Rot2 @432 (signed -0 normalized)
Trans X/Y @444; Z dropped
f2[4] @452
unknown ref pair @468
Enabled @476
```

Direct V4.4 selected check: 772/772 emitters reproduced by `particle_v2.py` physical mapping. Nonzero unknown-reference payload remains BLOCK until Golden evidence exists.

## Current real C++ M2 core

Repository already contains strict v264 reader + feature gate + animation metadata core and M2 probe. Next implementation target is the complete file-level writer rather than more broad scans.

## Immediate next step

Run only:

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

This checks only old V4.4 failure paths and emits exact remaining Playable + Sequence/Timeline differences. Do **not** run another 18k/11k broad scan.

Then:

```text
freeze V4.5/4.6 exceptional animation rules
 -> C++ Ribbon writer
 -> C++ Particle writer (physical v2 layout)
 -> whole M2 writer
 -> strict validator
 -> 1 Ribbon + 1 Particle real Turtle client regression
 -> feature-class batch conversion
```

WMO and complete ADT/WDT/WDL/DBC/MPQ integration are later phases; M2 completion does not mean all buildings/maps are already solved.
