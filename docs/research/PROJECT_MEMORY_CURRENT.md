# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 19:21 +08:00（V4.6 canonical-226 + whole-M2 assembly 阶段）

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

本文件是“模型移植”项目的当前入口。后续继续工作时优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1921.md
docs/research/M2_GOLDEN_REFERENCE_V46_REFINE_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_RIBBON_2026-08-19.md
```

## 最终目标

把 WoW 3.3.5a Build12340 的 M2/WMO/地图/BLP/DBC 等资源语义降版成 Vanilla 1.12.x / Turtle WoW 1.18.1 标准资源，并最终自动构建 Patch MPQ。M2 输出只能是标准 `MD20 v256`，不依赖 Orange/private runtime。

## V4.4 / V4.6 corpus 状态

```text
source M2                  18083
paired M2                  11596
high-risk deep              5036
deep errors                    0
AnimationLookup mismatch        0
old Playable V4 mismatch      498
```

V4.5 refinement：

```text
canonical226 already pass      453
canonical226 remaining          18
remaining mismatch records      20
remaining requested IDs          3
legacy noncanonical outputs     27
```

V4.6 新增 Golden overrides：

```text
28  -> 27
108 -> 111
112 -> 111
```

这三条解释 18/18 remaining canonical models、20/20 records。

27 个旧成功 target 使用非 canonical Playable count：

```text
203 : 20 models
1   :  7 models
```

它们保留为历史兼容证据，但 Turtle335Converter canonical writer **始终生成 226 records**。

## 当前 M2 基线

- `AnimationLookup`: **PRODUCTION_BASELINE**。
- `PlayableAnimationLookup`: **PRODUCTION_BASELINE_V46_CANONICAL_226**。
- `Sequence.Index`: 保留 source；不能写0，不能用 physical position 替代。
- Canonical Sequence：preserve source metadata + `3333` gap timeline。
- 29 个 Sequence/Timeline 历史 exceptions 中，27 个属于 noncanonical legacy outputs；另 2 个 canonical226 仅 bounds 不同。
- `.skin -> embedded View`: Golden-proven，C++ `SkinViewWriter` 已加入。
- ordinary 3D BLP: compatible 时原样复制。
- external `.anim`: validated cases 原样复制。
- Ribbon: C++ writer ready，Golden offline 420 emitters / 2520 tracks。
- Particle: C++ writer ready，Python V2 oracle 772 selected emitters；`nUnknownReference != 0` 继续 BLOCK。
- Classic/Turtle v256 strict validator：已存在。

## Playable V4.6

当前 canonical overrides：

```text
28  -> 27
108 -> 111
112 -> 111
121 -> 14
146 -> 0
172 -> 16
174 -> 16
181 -> 19
191 -> 159
```

仍然使用 model-aware recursive fallback；requested ID 自己存在时始终优先自身。

历史 LKBC fallback 只作为交叉参考。其代码明确把 `ID >= 226` 当作非旧动画 ID，并使用递归 fallback；旧转换器还存在清零 SubAnim/Index 的 lossy FIXME，因此不能作为最终字节目标。

## Sequence / Timeline V4.6 结论

29 个 exception target 全部 MD20 v256：

```text
Playable226 : 2
Playable203 : 20
Playable1   : 7
```

canonical226 两个模型仅 `bounds` 不同，Timeline/Playable 都通过。

其余27个存在典型旧式 lossy converter 特征：flags/index/timeline/bounds 被改写，少数 duration/playSpeed 改写，甚至存在 source1 sequence -> target9 sequences。

因此 canonical writer 继续 source-preserving 规则；旧式成功 target 证明客户端能容忍 lossy 变体，但不是我们应该复制的 canonical 输出。

## 当前 C++ M2 模块

```text
WotlkM2Reader
AnimationMetadata
LegacyTrack
SkinViewWriter
RibbonWriter
ParticleWriter
ClassicM2Validator
```

### SkinViewWriter

本轮新增：

```text
include/turtle335/m2/SkinViewWriter.h
src/m2/SkinViewWriter.cpp
tests/test_skin_view_writer.cpp
```

规则：

```text
WotLK SKIN 48B header -> Classic embedded View 44B header
indices               copy
triangles             copy
properties            copy
48B WotLK submesh     -> first 32B Classic submesh
24B TextureUnit       copy
multi-view            supported
absolute offsets      rewritten
```

本地 synthetic regression 已用：

```text
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror
```

编译 + 运行 PASS。CMake 已登记 source/test。

## 下一步（不要再广扫）

直接开发 whole-M2 writer / relocation pipeline：

```text
WotLK M2 v264 + .skin + AnimationData.dbc
        ↓
Classic/Turtle MD20 v256
        ↓
Sequence / AnimationLookup / Playable226
Bones / generic tracks
Vertices
Embedded Views
Colors / Transparency / TexAnim
Attachments / Events / Lights / Cameras
Ribbon / Particle
        ↓
ClassicM2Validator
        ↓
minimal Turtle 1.18.1 real-client regression
```

当前不需要用户再次提供普通模型或扫描结果。只有在 whole-M2 writer 离线生成通过后，才会要求最少量真实客户端验证样本。

ADT/WDT/WDL 与 WMO/完整 DBC/MPQ 自动化仍属于后续统一流水线阶段，不能把当前 M2 进度误称为“所有地图/建筑均完成”。
