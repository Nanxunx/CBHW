# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19（V4.5 C++ Effects + Target Validator / 精确 refinement 阶段）

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

本文件是“模型移植”项目的当前入口。优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1713.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_V45_EFFECTS.md
docs/research/M2_GOLDEN_REFERENCE_V45_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_RIBBON_2026-08-19.md
docs/research/M2_CPP_EFFECT_WRITERS_V45_2026-08-19.md
```

## 最终目标

把 WoW 3.3.5a Build12340 的 M2/WMO/地图/BLP/DBC 等资源语义降版成 Vanilla 1.12.x / Turtle WoW 1.18.1 标准资源，并最终自动构建 Patch MPQ。M2 输出只能是标准 `MD20 v256`。

## V4.4 corpus 状态

```text
source M2                  18083
paired M2                  11596
high-risk deep              5036
deep errors                    0
Sequence mismatch              29
Timeline mismatch              27
AnimationLookup mismatch        0
Playable old-V4 mismatch      498
true Ribbon models            268
Particle models              5570
```

## 当前 M2 基线

- `AnimationLookup`: **PRODUCTION_BASELINE**, 5036/5036 high-risk pass。
- `Sequence.Index`: 保留 source；不能写0，不能用 physical position 替代。
- `Playable`: **V4.5 provisional**。当前规则为 build12340 AnimationData field5 + Golden overrides + model-aware recursive fallback。
- `.skin -> embedded View`: Golden-proven。
- ordinary 3D BLP: compatible 时原样复制。
- external `.anim`: validated cases 原样复制。
- TextureAnimation: 已有统一 legacy track 路径。

## Playable V4.5

V4.4 自动选出的8个 `01_RuleMismatch` 成功112 target，当前 `playable_lookup_v45.py` 已做到：

```text
8 / 8 models PASS
226 / 226 records per model exact
```

仍需 `Run_ModelPort_Refine_V45.ps1` 对旧498 paths 精确重验后才能冻结最终 graph。

## Ribbon — Python GOLDEN_OFFLINE_READY / C++ 已接入

Golden：6 paired models / 420 emitters / 2520 tracks；static 420/420，track 2520/2520 PASS；WotLK176B -> Classic/Turtle220B。

Python oracle：

```text
tools/modelport/ribbon_converter_v45.py
tools/modelport/validate_ribbon_golden_v45.py
```

C++：

```text
include/turtle335/m2/RibbonWriter.h
src/m2/RibbonWriter.cpp
tests/test_ribbon_writer.cpp
```

已加入 CMake。修正 color 空轨默认 `(1,1,1)`、spline key-size 与必需 includes。隔离 Linux `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror` 编译 + regression PASS；完整 Windows/Actions 仍需继续确认。

## Particle V2 — C++ writer 已接入

Python oracle：`tools/modelport/particle_v2.py`。

selected Golden：10 pairs / 772 emitters / 8492 versioned tracks；source476B -> target504B。

C++：

```text
include/turtle335/m2/ParticleWriter.h
src/m2/ParticleWriter.cpp
tests/test_particle_writer.cpp
```

已加入 CMake。覆盖 flags16-bit downgrade、filename relocation、10 float tracks、Fake color/alpha/size、head/tail cells、504B target tail、Rot2 -0 normalization、Trans XY、enabled legacy track。`nUnknownReference != 0` 继续硬性 BLOCK。隔离 Linux strict GCC 编译 + synthetic regression PASS。

Particle target tail：

```text
332 midpoint
336 BGRA[3]
348 size[3]
360 10 cell shorts
380 unk Vec3
392 scales Vec3
404 slowdown
408 rotation
412 unknown4 Vec2 = 0
420 Rot1 Vec3
432 Rot2 Vec3
444 Trans X,Y only
452 f2[4]
468 unknown ref pair
476 enabled track
504 end
```

## Classic/Turtle v256 Strict Validator

已新增并接入 CMake：

```text
include/turtle335/m2/ClassicM2Validator.h
src/m2/ClassicM2Validator.cpp
tests/test_classic_m2_validator.cpp
```

只验证已有 Golden 证据的结构，不猜未知块：MD20/version256、animations68、AnimationLookup、Playable226、bones108、vertices48、embedded View44 + child arrays、textures16、TexAnim84、lookup arrays、Ribbon220、Particle504。

## 当前代码方向

```text
WotlkM2Reader
 -> AnimationMetadata / Playable
 -> LegacyTrack
 -> RibbonWriter
 -> ParticleWriter
 -> whole-M2 v256 writer（下一组合阶段）
 -> ClassicM2Validator
 -> minimal Turtle 1.18.1 regression
```

最终用户工具必须输出标准 MD20 v256，不依赖 Orange/private runtime。

## 当前唯一需要用户提供的输入

无需再上传普通模型，也不要重新跑全库扫描。请运行：

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

它只读取 V4.4 已知失败路径。上传：

```text
E:\ModelPort_GoldenUpload_V45_Refine\ModelPort_GoldenReference_V45_Refine_ALL.zip
```

收到后立即：

```text
freeze remaining Playable graph
 -> classify 29/27 Sequence/Timeline exceptions
 -> lock effect writer rules
 -> whole-M2 v264 + skin -> v256 serializer
 -> ClassicM2Validator
 -> 最少量 Turtle 1.18.1 实机 regression
```

ADT/WDT/WDL 与 WMO/完整 DBC/MPQ 自动化属于后续统一流水线阶段；当前不能把 M2 进度误称为“所有地图/建筑均完成”。
