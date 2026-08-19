# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19（V4.5 Ribbon Golden 阶段）

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

## Playable V4.5 最新验证

V4.4 自动选出的 8 个 `01_RuleMismatch` 成功112 target，使用当前 `playable_lookup_v45.py` 后：

```text
8 / 8 models PASS
226 / 226 records per model exact
```

旧 V4 的核心错误包括 `40->0`、`121->0` 以及 170 系过早直接落到16；当前 V4.5 的 model-aware fallback 能按模型已有 AnimationID 解析到19/17/16。

仍需运行 `Run_ModelPort_Refine_V45.ps1` 对旧498 paths 精确重验后才能冻结最终 graph。

## Ribbon — GOLDEN_OFFLINE_READY

V4.4 6组真正 Ribbon Golden 已完成逐字节/语义核验：

```text
Golden models                6
RibbonEmitters             420
animation tracks          2520
static mapping pass        420/420
track semantic pass       2520/2520
```

结构：

```text
WotLK v264 RibbonEmitter   176B
Classic/Turtle v256        220B
```

新增：

```text
tools/modelport/ribbon_converter_v45.py
tools/modelport/validate_ribbon_golden_v45.py
tools/modelport/test_ribbon_converter_v45.py
```

实际生成的 Classic Ribbon block 再解析后，对 420 emitters / 2520 tracks 全部与成功112 Golden semantic match。Ribbon 不再继续宽泛研究；下一步直接迁移到 C++ whole-M2 writer，再做一个 Turtle 1.18.1 实机 Ribbon regression。

## Particle

当前物理 writer oracle：

```text
tools/modelport/particle_v2.py
```

V4.4 selected 10 Particle Golden pairs / 772 emitters 已重新 byte-level 核验。确认：

```text
source stride 476
target stride 504
772 selected emitters mapped
```

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

nonzero unknown-reference 仍 BLOCK，直到得到成功 Golden。

## 当前代码方向

Python Golden/effect writer 是算法 oracle；生产实现迁移进 C++：

```text
Reader
 -> normalized semantic model
 -> downgrade policy
 -> v256 writer
 -> strict validator
 -> minimal real-client regression
```

C++ M2 core 已有 `WotlkM2Reader`、`AnimationMetadata` 等基础组件。

## 当前唯一需要用户执行的本地 refinement

运行：

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

只读取 V4.4 已知失败路径，不广扫全库。上传：

```text
E:\ModelPort_GoldenUpload_V45_Refine\ModelPort_GoldenReference_V45_Refine_ALL.zip
```

收到后立即：

```text
freeze remaining Playable graph
 -> classify 29/27 Sequence/Timeline exceptions
 -> C++ Ribbon writer
 -> C++ Particle V2 writer
 -> whole-M2 v264+skin -> v256 writer
 -> strict validator
 -> minimal Turtle 1.18.1 regression
```

ADT/WDT/WDL 与 WMO/完整 DBC/MPQ 自动化属于后续阶段，不能把当前 M2 进度误称为“所有地图/建筑均完成”。
