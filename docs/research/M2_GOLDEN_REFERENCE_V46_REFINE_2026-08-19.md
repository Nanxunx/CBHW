# M2 Golden Reference V4.6 — Playable refinement closure

日期：2026-08-19

输入：`ModelPort_GoldenReference_V45_Refine_ALL.zip`

## 1. V4.5 refinement 结果

```text
V4.4 deep rows                         5036
V4.4 Playable fail models              498
V4.4 Sequence/Timeline exception rows   29
Targeted unique models                 500
V4.5 errors                              27
V4.5 Playable pass models              453
V4.5 remaining Playable fail models     18
Remaining mismatch records              20
Remaining requested IDs                  3
Sequence/Timeline detail models          29
Sequence/Timeline detail rows            40
```

剩余 20 条 Playable mismatch 只包含 3 个 RequestedID：

```text
28  -> Golden 27   (5 occurrences)
108 -> Golden 111  (5 occurrences)
112 -> Golden 111 (10 occurrences)
```

所有 20 条中 Golden flags 都为 0，且目标 AnimationID 都真实存在于对应模型。

因此 V4.6 新增 canonical-226 Golden overrides：

```text
28  -> 27
108 -> 111
112 -> 111
```

将这三条加入后，18/18 remaining models、20/20 mismatch records 被完整解释。

## 2. 498 个旧 Playable fail 的最终分类

旧 V4.4 的 498 个 Playable fail 现在分成两类：

### Canonical 226 outputs

```text
V4.5 already pass        453
V4.6 newly resolved       18
----------------------------
canonical resolved       471
```

这 471 个成功目标均使用 226-entry `PlayableAnimationLookup`，当前 V4.6 model-aware fallback graph 已完全解释。

### Legacy noncanonical outputs

剩余 27 个并不是当前 graph 仍然错误，而是成功历史 target 自身的 `PlayableAnimationLookup` count 不是 226：

```text
count = 203   20 models
count =   1    7 models
```

这 27 个全部同时属于 V4.4 的 Sequence/Timeline exception 集。它们保留为“旧转换器/兼容输出证据”，但不作为 Turtle335Converter canonical writer 的字节目标。

**Canonical Turtle policy：始终生成 226 records。**

## 3. Sequence / Timeline 29 个 exceptions

29 个 exception target 全部是 MD20 v256：

```text
Playable count 203 : 20 models
Playable count   1 :  7 models
Playable count 226 :  2 models
```

40 条 Sequence detail 的字段差异统计：

```text
bounds          39
flags           37
start           37
end             37
index           11
length           4
playSpeed        4
sequence_count   1
```

其中 canonical-226 的 2 个模型：

```text
Hand_1H_Naxxramas_D_01Right
SC_BodyCart_01
```

都只有 `bounds` 与 source 不同，Timeline 完全通过，Playable 也通过。

其余 27 个 noncanonical target 明显具有旧转换器/手工降级特征，例如：

- flags 被清零/简化；
- Index 被清零；
- Timeline 从 0 或其他自定义位置开始；
- bounds 被替换；
- 少数 duration/playSpeed 被改写；
- `TS_FishingPet_01` 甚至出现 source 1 sequence -> target 9 sequences。

因此这些输出证明“1.12 客户端可以接受更 lossy 的旧式 M2”，但**不能推翻当前 source-preserving canonical writer 规则**。

Canonical writer 继续使用：

```text
Sequence metadata: preserve source
Sequence.Index: preserve source aliasNext semantics
Timeline: +3333 gap, then source duration
```

bounds 允许未来做兼容归一化，但默认仍优先 preserve source。

## 4. V4.6 状态

### Production baseline

```text
AnimationLookup       PRODUCTION_BASELINE
Playable canonical226 PRODUCTION_BASELINE_V46
Sequence policy        CANONICAL_SOURCE_PRESERVE
Embedded View          GOLDEN_PROVEN / C++ writer added
Ribbon                 GOLDEN_OFFLINE_READY / C++ writer exists
Particle               GOLDEN_OFFLINE_READY_V2 / C++ writer exists
```

### Playable Golden overrides

当前 build12340 `AnimationData.dbc field5` 上的 canonical overrides：

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

仍然使用 model-aware recursive fallback：如果 requested ID 本身存在，则直接使用自身，不受 override 影响。

## 5. 本轮代码变化

- Python `playable_lookup_v45.py` 升级为 V4.6 canonical-226 baseline。
- C++ `AnimationMetadata.cpp` 同步 28/108/112 overrides。
- `test_m2_core.cpp` 增加 V4.6 regression。
- 新增 C++ `SkinViewWriter`：
  - `SKIN` 48B header -> embedded Classic View 44B header；
  - indices / triangles / properties 原样复制；
  - WotLK 48B submesh -> Classic 32B submesh（前 32B）；
  - 24B TextureUnit 原样复制；
  - 支持多个 skin/view；
  - 输出绝对 M2 offsets。

## 6. 下一步

无需再次广扫。

直接进入 whole-M2 writer 组装阶段：

```text
WotLK M2 v264
+ external .skin
+ AnimationData.dbc
        ↓
Canonical Classic/Turtle header v256
Sequence + AnimationLookup + Playable226
Bones / tracks
Vertices
Embedded Views
Materials / Texture / TexAnim
Attachments / Events / Lights / Cameras
Ribbon
Particle
        ↓
ClassicM2Validator
        ↓
minimal real-client regression
```

下一阶段优先实现 whole-M2 block relocation/assembly，再补尚未迁入 C++ 的 Bone/Color/Transparency/TexAnim/Attachment/Event/Light/Camera 通用 track blocks。