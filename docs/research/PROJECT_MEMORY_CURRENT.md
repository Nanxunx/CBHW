# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19（V4.5 Effect Writer / 精确 refinement 阶段）

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

本文件是“模型移植”项目的当前入口。优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1713.md
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

V4.4 自动选出的 8 个 `01_RuleMismatch` 成功112 target，使用当前 `playable_lookup_v45.py`：

```text
8 / 8 models PASS
226 / 226 records per model exact
```

旧 V4 的核心错误包括 `40->0`、`121->0` 以及 170 系过早直接落到16；当前 V4.5 model-aware fallback 能按模型实际拥有的 AnimationID 解析到19/17/16。

仍需运行 `Run_ModelPort_Refine_V45.ps1` 对旧498 paths 精确重验后才能冻结最终 graph。

## Ribbon — Python GOLDEN_OFFLINE_READY / C++ 已接入

Golden：

```text
6 paired models
420 RibbonEmitters
2520 animation tracks
static mapping       420/420 PASS
track semantics     2520/2520 PASS
WotLK stride             176B
Classic/Turtle stride    220B
```

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

本轮已把 `LegacyTrack.cpp` / `RibbonWriter.cpp` 正式加入 CMake。C++ Ribbon writer 已修正 color 空轨默认 `(1,1,1)`、spline key-size 处理以及必需 includes。隔离的 Linux `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror` 编译 + regression 已 PASS；完整 GitHub Actions/Windows 构建仍需继续观察，不能提前宣称全平台 production-ready。

## Particle V2 — C++ writer 已开始生产化

Python oracle：

```text
tools/modelport/particle_v2.py
```

selected Golden：

```text
10 paired models
772 ParticleEmitters
8492 versioned animation tracks
source stride 476B
target stride 504B
```

C++ 新增：

```text
include/turtle335/m2/ParticleWriter.h
src/m2/ParticleWriter.cpp
tests/test_particle_writer.cpp
```

已接入 CMake。当前实现覆盖：flags16-bit downgrade、filename relocation、10 float tracks、Fake color/alpha/size、head/tail cells、最终 504B tail layout、Rot2 -0 normalization、Trans XY、enabled legacy track。`nUnknownReference != 0` 继续硬性 BLOCK，禁止猜测。

隔离 Linux `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror` 编译 + synthetic regression 已 PASS。完整 Golden/CMake/Windows CI 仍需继续验证。

最终 target tail layout：

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

## 当前代码方向

```text
WotlkM2Reader
 -> AnimationMetadata / Playable
 -> LegacyTrack
 -> RibbonWriter
 -> ParticleWriter
 -> whole-M2 v256 writer（下一组合阶段）
 -> strict validator
 -> minimal Turtle 1.18.1 regression
```

Python Golden writer 继续作为算法 oracle；最终用户工具必须走标准 C++/标准 MD20 v256，不依赖 Orange/private runtime。

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
 -> lock C++ effect writers
 -> whole-M2 v264 + skin -> v256 serializer
 -> strict validator
 -> 最少量 Turtle 1.18.1 实机 regression
```

ADT/WDT/WDL 与 WMO/完整 DBC/MPQ 自动化属于后续统一流水线阶段；当前不能把 M2 进度误称为“所有地图/建筑均完成”。
