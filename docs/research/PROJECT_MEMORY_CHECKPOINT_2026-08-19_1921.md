# 模型移植项目记忆检查点 — 2026-08-19 19:21 +08:00

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

## 本轮用户输入

已收到并分析：

```text
ModelPort_GoldenReference_V45_Refine_ALL.zip
```

## Playable V4.6 结论

V4.4 旧失败：498 models。

V4.5 refinement：

```text
453 models already pass
18 models still mismatch
20 mismatch records
3 requested IDs only
27 tool errors
```

20 条剩余 mismatch：

```text
28  -> 27   x5
108 -> 111  x5
112 -> 111 x10
```

加入 overrides 后 18/18 / 20/20 可解释。

27 个 tool errors 并非 graph 未解决，而是其成功 target 的 Playable count 为：

```text
203 : 20 models
1   :  7 models
```

这 27 个全部属于 Sequence/Timeline exception。Canonical Turtle writer 不复制这种历史变体，继续强制 226 records。

因此当前：

```text
AnimationLookup        PRODUCTION_BASELINE
Playable canonical226  PRODUCTION_BASELINE_V46
```

新增 Golden overrides：

```text
28  -> 27
108 -> 111
112 -> 111
```

原 overrides 继续保留：121->14, 146->0, 172->16, 174->16, 181->19, 191->159。

## Sequence / Timeline exceptions

29 个 exception target：

```text
Playable226 : 2
Playable203 : 20
Playable1   : 7
```

canonical226 的 2 个仅 bounds 不同；Timeline/Playable 都正常。

其余 27 个表现出旧式 lossy converter 特征：flags/index/timeline/bounds 等被改写，少数 duration/playSpeed 也改变，甚至存在 source1 sequence -> target9 sequences。

所以 canonical writer 继续：

```text
preserve source Sequence metadata
preserve Sequence.Index
3333-gap timeline
```

## 新增 C++ 工作

本轮新增：

```text
include/turtle335/m2/SkinViewWriter.h
src/m2/SkinViewWriter.cpp
tests/test_skin_view_writer.cpp
```

实现：

```text
WotLK external SKIN 48B header
→ Classic/Turtle embedded View 44B header

indices           copy
triangles         copy
properties        copy
48B LK submesh    -> first 32B Classic submesh
24B TextureUnit   copy
multi-view        supported
absolute offsets  rewritten
```

本地用 `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror` 编译并运行 synthetic regression：PASS。

CMake 已登记 SkinViewWriter source/test。

## 当前 M2 主线

```text
Sequence             ready
AnimationLookup      production
Playable226          production V4.6
LegacyTrack          ready
Skin->EmbeddedView   C++ ready
Ribbon               C++ writer ready
Particle             C++ writer ready
Classic validator    ready
```

仍需 whole-M2 assembly 中的通用 block：

```text
Bones
Colors
Transparency
TexAnim
Attachments
Events
Lights
Cameras
Texture/material lookup blocks
whole-header relocation
```

## 下一步

不再要求用户扫描。

直接开发：

```text
ClassicM2Writer / whole-M2 relocation pipeline
```

完成可离线生成完整 v256 后，只做最小真实 Turtle 1.18.1 regression，而不是逐模型反复测试。
