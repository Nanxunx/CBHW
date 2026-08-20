# 模型移植项目记忆检查点 — 2026-08-19 19:57 +08:00

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

## 本轮外部参考

深度分析：

```text
Wall-core/M2Workshop
Wallcraft M2 Workshop.1sc
```

完整结论：

```text
docs/research/WALLCRAFT_M2_WORKSHOP_ANALYSIS_2026-08-19.md
```

Wallcraft 是 010 Editor 的同版本 M2 修改器，不是 WotLK->Vanilla/Turtle retroport converter。不能替代 Golden，但对以下内容价值很高：

```text
v264/v256 fixed structure cross-check
Bone/Color/Transparency/TexAnim/Attachment/Event/Light/Camera offsets
Ribbon/Particle cross-check
WotLK nested track semantics
external .anim sidecar autoload
Vanilla float C4Vector quaternion evidence
```

根目录未见 LICENSE；本项目不复制其脚本代码，只独立实现被 Golden/其他实现交叉确认的格式事实。

## 本轮新增 C++

```text
include/turtle335/m2/ExternalAnim.h
src/m2/ExternalAnim.cpp
tests/test_external_anim.cpp

include/turtle335/m2/QuaternionCodec.h
src/m2/QuaternionCodec.cpp
tests/test_quaternion_codec.cpp

include/turtle335/m2/BoneWriter.h
src/m2/BoneWriter.cpp
tests/test_bone_writer.cpp
```

均已加入 CMake。

### ExternalAnim

正式实现 WotLK per-sequence sidecar 数据源解析：

```text
sequence.flags & 0x20 != 0 -> payload in main M2
sequence.flags & 0x20 == 0 -> payload in <Stem><AnimID:04>-<SubID:02>.anim
```

Outer Times/Keys ArrayRef 和 inner count/offset pair 仍从 main M2 读取；inner payload 的 offset 在 external sequence 上解释为 .anim 内 offset。

这使后续 Bone/Color/etc writer 能把外置动画真正读回并 flatten 进 legacy v256，而不是只把 .anim 作为旁路文件复制。

### QuaternionCodec

正式固化：

```text
WotLK compressed quaternion: 4 * int16 = 8B
Classic/Turtle quaternion:   4 * float = 16B
spline key:                  24B -> 48B
```

Golden 特例：

```text
source component -1 -> exact +1.0f
```

继续使用已经由 Crossbow/Golden 验证过的 signed-short 公式，不使用 Wallcraft 的 generic unsigned helper 覆盖 Golden。

### LegacyTrack policy correction

`FlattenLegacyValueTrack` 新增：

```text
LegacySingleKeyPolicy::PerSequence
LegacySingleKeyPolicy::ConstantNoRanges
```

原因：

```text
Ribbon / Particle:
  one sequence + one outer + one key
  -> per-sequence ranges/start-end expansion

Bone / Color / Transparency / TexAnim / Attachment / Light / Camera:
  one outer + one key
  -> historical constant track, no ranges
```

这避免把一个 Ribbon/Particle 特例错误推广到全部 legacy block。

### BoneWriter

实现：

```text
WotLK Bone 88B -> Classic/Turtle Bone 108B
```

规则：

```text
source +0..11  -> target +0..11
source unknown int32 +12 dropped
translation +16 20B -> target +12 28B
rotation    +36 20B -> target +40 28B
scaling     +56 20B -> target +68 28B
pivot       +76     -> target +96
```

Rotation 使用 QuaternionCodec；empty scaling group 使用 `(1,1,1)` identity，而不是 zero scale。

## 对用户 V4.5 refinement Golden 的额外交叉检查

使用用户上传的 `ModelPort_GoldenReference_V45_Refine_ALL.zip` 中 41 组 paired M2 做本轮结构核验。

在 source/target bone count 相同的样本中：

```text
source prefix + pivot direct-match observations : 1888
translation semantic matches                    : 1607
compressed quaternion -> float4 exact matches   : 1222
```

大量剩余 rotation track 无法在该精简包中重验，是因为某些模型实际引用的 source .anim family 未被 refinement copy_family 打包；这不是 codec mismatch。

Bone scale 空组也出现了真实 Golden：identity `(1,1,1)` 能匹配成功 target，而 zero default 不能，因此 BoneWriter 采用 identity scale。

旧成功 target 中仍存在少数 lossy bone filtering/metadata rewrite；canonical writer 不复制这种历史 lossy 行为，继续 source-preserving。

## 当前 canonical M2 状态

```text
AnimationLookup                    PRODUCTION_BASELINE
PlayableAnimationLookup226         PRODUCTION_BASELINE_V46
Sequence metadata/Index            source-preserving
Classic 324B header                C++ ready
Skin -> embedded View              C++ ready
External .anim resolver            C++ ready
Quaternion codec                   C++ ready
Bone writer                        C++ integrated
Ribbon writer                      C++ integrated
Particle V2 writer                 C++ integrated
Classic v256 validator             C++ ready
```

## 下一步

不再广扫。

直接继续 whole-M2 block writers：

```text
Color / Transparency
TextureAnimation
Attachment
Event (special no-key track)
Light
Camera
```

随后：

```text
whole-M2 relocation assembly
 -> MD20 v256
 -> ClassicM2Validator
 -> 最小真实客户端 regression
```

Wallcraft/M2Workshop 不会成为最终工具运行依赖。最终 Turtle335Converter 仍是独立 C++ converter。
