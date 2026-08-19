# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 16:23 +08:00

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

`WYTurtle` 不是模型转换器源码仓库。

## 当前目标

把 WoW 3.3.5a build12340 的模型/地图/DBC 等资源语义降版为 Vanilla 1.12.x / Turtle WoW 1.18.1 可直接使用的标准资源，最终形成实际转换工具。

M2 最终输出只允许标准 `MD20 v256`；Orange/private wrapper 仅可作算法参考，不允许成为输出格式或客户端依赖。

## 当前 Golden 数据

```text
335 raw:
E:\335_FinalExtract_V5

成功 1.12 Golden:
E:\335to112_Converted_FinalExtract_V1
```

V4.4 定向扫描：

```text
source M2                  18083
paired M2                  11596
high-risk deep              5036
deep errors                    0
Sequence mismatch              29
Timeline mismatch              27
AnimationLookup mismatch        0
Playable V4 mismatch          498
true Ribbon models            268
Particle models              5570
TexAnim models                909
external .anim models         395
Alias models                  934
SubAnimation models           822
duplicate AnimID models       822
```

## M2 当前生产规则

### Sequence

`Sequence.Index` 必须保留 WotLK source；不能写0，也不能等同 physical array position。

普通 Classic timeline 规则仍为每条 sequence 前 `+3333ms`，`end=start+source.length`。V4.4 有 29/27 个少量特殊例外，下一包只针对这些模型做精确 Golden diff。

### AnimationLookup — PRODUCTION_BASELINE

5036/5036 high-risk Golden PASS：

```text
count=max(AnimationID)+1
missing=0xFFFF
duplicate AnimationID:
  first SubAnimationID==0
  else first physical occurrence
```

### PlayableAnimationLookup — V4.5 provisional

旧 V4 hardcoded graph 已废弃。

主 fallback graph：build12340 `AnimationData.dbc` raw field index 5；按当前模型实际拥有的 AnimationID 递归解析。

Golden corrections：

```text
121->14
146->0
172->16
174->16
181->19
191->159
```

29 个已上传 selected Golden / 6554 Playable records exact PASS。仍须运行 V4.5 refinement 对旧498 failures做一次局部重验后才能升 production。

### Ribbon — GOLDEN_READY_OFFLINE_SEMANTIC

```text
WotLK 176B -> Classic 220B
13 models / 448 emitters exact PASS
```

6条 track 使用通用 legacy Range/Times/Keys：color, opacity, heightAbove, heightBelow, texSlot, enabled。下一步是完整 M2 writer 集成 + 1个真实客户端 Ribbon 回归。

### Particle — GOLDEN_READY_OFFLINE_SEMANTIC_WITH_BLOCKER

```text
WotLK 476B -> Classic 504B
24 models / 930 emitters exact PASS
```

10条 main float track 全部 930/930 exact；Fake gradient/size/tiles 规则已收敛；enabled 默认key=1。

nonzero `nUnknownReference/ofsUnknownReference` 尚无 Golden，必须 BLOCK。

成功 target 会丢弃若干 WotLK-only semantics，工具必须报告显式 LOSS，而不是静默：ParticleColorIndex、unknown1/2、scale-vary、unknown3/4。

### 其它已稳定规则

- quaternion short -1 -> exact +1.0f
- generic Legacy Range/Times/Keys
- external `.skin -> embedded View`
- ordinary compatible 3D BLP 原样复制
- validated external `.anim` 原样复制
- ItemDisplayInfo 25->23 语义映射；最终使用单一 cumulative Master DBC

## 工具化进展

仓库过去只有 ADT/DBC C++ core。当前开始正式加入 M2 C++ core：

```text
include/turtle335/m2/WotlkM2Reader.h
src/m2/WotlkM2Reader.cpp
include/turtle335/m2/AnimationMetadata.h
src/m2/AnimationMetadata.cpp
tools/probe_wotlk_m2.cpp
tests/test_m2_core.cpp
```

已实现：严格 v264 reader、feature gate、Sequence record builder、AnimationLookup、build12340 AnimationData parser、Playable V4.5 generator、M2 CLI probe。

本地 C++17 严格编译已用 `-Wall -Wextra -Wpedantic -Werror` 验证，M2 core test PASS；真实 Ribbon Golden source probe 也已运行。

Python Golden writer：

```text
tools/modelport/ribbon_v1.py
tools/modelport/particle_v1.py
tools/modelport/playable_lookup_v45.py
```

## 立即下一步

1. 运行 `Run_ModelPort_Refine_V45.ps1`：只重验旧498 Playable failures，并打包29/27 Sequence/Timeline exceptions；不要重扫18083模型。
2. 将 Ribbon/Particle Golden writer 迁入 C++ full M2 builder。
3. 实现完整 `v264 + .skin -> v256 embedded View` 文件级 writer/validator。
4. Ribbon + Particle 各做一个 Turtle 1.18.1 真客户端 Golden regression。
5. 按 feature gate 批量转换。

详细 V4.5 研究：

```text
docs/research/M2_GOLDEN_REFERENCE_V45_2026-08-19.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1623.md
```

ADT 详细历史仍读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
```
