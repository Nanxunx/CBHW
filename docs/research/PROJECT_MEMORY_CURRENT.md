# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 17:13 +08:00

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

本文件是“模型移植”项目的当前入口。后续继续工作时，优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1713.md
docs/research/M2_GOLDEN_REFERENCE_V45_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
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

- `AnimationLookup`: **PRODUCTION_BASELINE**, 5036/5036 high-risk pass.
- `Sequence.Index`: 保留 source；不能写0，不能用 physical position 替代。
- `Playable`: **V4.5 provisional**，build12340 AnimationData field5 + Golden overrides + model-aware recursive fallback；只需局部重验旧498 failures。
- `.skin -> embedded View`: Golden-proven。
- ordinary 3D BLP: compatible 时原样复制。
- external `.anim`: validated cases 原样复制。
- Ribbon: 176B -> 220B，Golden-ready offline，待 full-M2/C++ integration + real client regression。
- Particle: 476B -> 504B，tracks/gradient/cells 已收敛；nonzero unknown-reference 仍 BLOCK。

## 2026-08-19 最新物理写入修正

对 V4.4 selected 10 Particle Golden pairs / 772 emitters 做了重新 byte-level 核验。最终 v256 tail layout：

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
432 Rot2 Vec3 (normalize -0 -> +0)
444 Trans X,Y only
452 f2[4]
468 unknown ref pair
476 enabled track
504 end
```

因此旧 `particle_v1.py` 的 `Rot1@416/Rot2@428/Trans@440` 不能作为最终 physical writer。新增 `particle_v2.py` 使用直接 Golden 字节布局；selected 772/772 输出匹配。

## 当前代码进展

C++ M2 core 已有：

```text
include/turtle335/m2/WotlkM2Reader.h
src/m2/WotlkM2Reader.cpp
include/turtle335/m2/AnimationMetadata.h
src/m2/AnimationMetadata.cpp
tools/probe_wotlk_m2.cpp
tests/test_m2_core.cpp
```

Python Golden/effect writer 继续作为算法 oracle；下一阶段迁移进 C++ whole-M2 writer。

## 下一步（不要广扫）

运行：

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

它只读取 V4.4 已知失败路径，输出剩余 Playable requested-ID 精确差异和 29/27 Sequence/Timeline 字段差异。完成后冻结异常规则并直接进入 C++ Ribbon/Particle + full M2 writer。

ADT/WDT/WDL 与 WMO/完整 DBC/MPQ 自动化属于后续阶段，不能把当前 M2 进度误称为“全部地图/建筑已经完成”。
