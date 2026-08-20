# 模型移植项目记忆检查点 — V4.5 C++ Effects + Target Validator

日期：2026-08-19

权威仓库：`NansenCore/Turtle335Converter` / `main`

## 已完成

### Playable V4.5 selected recovery

V4.4 自动选出的8个最高优先级 RuleMismatch 成功112 target，当前 `playable_lookup_v45.py` 已做到 8/8、每模型226/226 exact。仍需只对旧498路径做 focused refinement 才能冻结剩余 graph。

### Ribbon

Golden：6 paired models / 420 emitters / 2520 tracks。

Python `ribbon_converter_v45.py`：GOLDEN_OFFLINE_READY。

C++ `RibbonWriter.cpp` 已修正并加入 CMake；新增 `tests/test_ribbon_writer.cpp`。隔离 Linux GCC17 + `-Wall -Wextra -Wpedantic -Werror` 编译和 regression PASS。

### Particle V2

Python `particle_v2.py`：selected Golden oracle，10 pairs / 772 emitters / 8492 versioned tracks；source476B -> target504B。

C++ 新增 `ParticleWriter.h/.cpp` + `test_particle_writer.cpp`，已加入 CMake。当前明确 BLOCK `nUnknownReference != 0`。隔离 Linux GCC17 + strict warnings 编译和 synthetic regression PASS。

### Classic/Turtle target validator

新增：

```text
include/turtle335/m2/ClassicM2Validator.h
src/m2/ClassicM2Validator.cpp
tests/test_classic_m2_validator.cpp
```

并加入 CMake。它只验证已经有 Golden 证据的 v256 固定块，不猜未知结构：MD20/version256、name、animations68、AnimationLookup、Playable226、bones108、vertices48、embedded View44 + child arrays、textures16、TexAnim84、lookup arrays、Ribbon220、Particle504。

## 当前唯一需要用户提供

不需要普通模型，不需要重新扫描全库。

运行 `tools/modelport/Run_ModelPort_Refine_V45.ps1`，上传：

```text
<V45_GOLDEN_ROOT>\ModelPort_GoldenReference_V45_Refine_ALL.zip
```

只处理 V4.4 已知498 Playable mismatch 和 29/27 Sequence/Timeline exceptions。

## 收到后立即继续

```text
冻结 Playable graph
 -> 精确归类29/27 Sequence/Timeline
 -> effect writer rule lock
 -> whole-M2 v264 + skin -> v256 assembler
 -> ClassicM2Validator
 -> 1个Ribbon + 1个Particle真实1.18.1 regression
 -> batch M2 conversion
```

注意：WMO 与完整地图/DBC/MPQ自动化仍是后续阶段，不能把当前M2进度误记为整个资产体系100%完成。
