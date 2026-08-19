# 模型移植项目记忆检查点 — 2026-08-19 19:57 R2

权威仓库：`NansenCore/Turtle335Converter` / `main`。

## Wallcraft 分析后的实际推进

外部参考：`Wall-core/M2Workshop`。它是 010 Editor 同版本编辑器，不是 retroport converter；高价值证据集中在 fixed layouts、WotLK nested tracks、external `.anim` 与 Vanilla float quaternion。完整分析见：

```text
docs/research/WALLCRAFT_M2_WORKSHOP_ANALYSIS_2026-08-19.md
```

## 新增/完成 C++ 模块

```text
ExternalAnim
QuaternionCodec
BoneWriter
AuxiliaryTrackWriters
CameraWriter
EventWriter
LightWriter (REFERENCE_GATED)
ClassicM2Writer (whole-M2 canonical assembler)
```

`WotlkM2Reader` 已扩展为暴露完整 v264 304B fixed header arrays。

Ribbon/Particle writer 均已接入 external `.anim` nested payload resolver。

Attachment empty enabled track 已按 V4.4 real paired Golden 修正为：

```text
Ranges=[]
Times=[0]
Keys=[1]
```

## Whole-M2 assembler

新增：

```text
include/turtle335/m2/ClassicM2Writer.h
src/m2/ClassicM2Writer.cpp
tests/test_classic_m2_writer.cpp
```

当前 assembly 路径：

```text
v264 source
+ skins
+ build12340 AnimationData.dbc
+ optional per-sequence .anim bytes
   ↓
324B v256 header
name + NUL
GlobalSequences
Sequence68
AnimationLookup
Playable226
Bone108
KeyBoneLookup
Vertex48
embedded Views
Color56
Texture defs + relocated names
Transparency28
TexAnim84
static lookup/render/bounds arrays
Attachment48
Event44
Light212 (default blocked by gate)
Camera124
Ribbon220
Particle504
   ↓
ClassicM2Validator
```

Unknown/lossy classes fail closed rather than silently stripping.

## Generic one-key representation nuance

Historic successful112 corpus contains both legal styles for some ordinary value tracks:

```text
ConstantNoRanges
PerSequence start/end expansion
```

So this is not a universal format invariant. The code keeps an explicit `LegacySingleKeyPolicy`. Canonical writer chooses deterministic policies per block while real Turtle/112 Golden remains the top oracle.

## Light data request

Current selected Golden archives contain no non-zero Light paired family. `LightWriter` therefore remains `REFERENCE_GATED` despite Wallcraft+Coffee structure agreement.

Added targeted collector only for this missing class:

```text
tools/modelport/Collect_Light_Golden_V46.py
tools/modelport/Run_Collect_Light_Golden_V46.ps1
```

It reads only M2 headers and packages at most 5 same-path pairs where source and target both have Light>0. No deep/full scan.

## 下一步

1. Run build/CTest on current whole-writer branch/main commits.
2. If Light targeted pack is supplied, freeze/repair Light semantics.
3. Run generated whole-M2 against selected static/animated/Ribbon/Particle Golden families.
4. Strengthen `ClassicM2Validator` for newly integrated Color/Transparency/Attachment/Event/Light/Camera blocks.
5. Produce minimal Turtle 1.18.1 regression pack: one complex Ribbon + one Particle/Creature.
6. After M2 is production-locked, continue WMO/ADT/WDT/WDL/DBC/MPQ integration.
