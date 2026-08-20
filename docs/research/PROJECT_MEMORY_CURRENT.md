# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 19:57 +08:00（V4.6 whole-M2 canonical assembler）

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

后续继续工作优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1957_R2.md
docs/research/WALLCRAFT_M2_WORKSHOP_ANALYSIS_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V46_REFINE_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_RIBBON_2026-08-19.md
```

## 最终目标

把 WoW 3.3.5a Build12340 的 M2/WMO/地图/BLP/DBC 等资源语义降版成 Vanilla 1.12.x / Turtle WoW 1.18.1 标准资源，并最终自动构建 Patch MPQ。M2 输出只允许标准 `MD20 v256`，不依赖 Orange、010 Editor 或其他 private runtime。

## Canonical animation baseline

```text
AnimationLookup              PRODUCTION_BASELINE
PlayableAnimationLookup      PRODUCTION_BASELINE_V46_CANONICAL_226
Sequence.Index               preserve source
Sequence timeline            +3333 before each sequence + source duration
```

canonical overrides：

```text
28->27
108->111
112->111
121->14
146->0
172->16
174->16
181->19
191->159
```

V4.5 refinement 已把旧498 Playable mismatch 收敛；27个旧 target 使用 Playable203/1，属于 noncanonical/lossy converter output，不作为 writer 目标。

## Wallcraft M2 Workshop

`Wall-core/M2Workshop` 是 010 Editor 的 Vanilla/TBC/WotLK **同版本 M2 编辑脚本**，不是 3.3.5a -> Vanilla/Turtle retroport converter。

最有价值：

```text
fixed layouts / offsets cross-check
Bone/Color/Transparency/TexAnim/Attachment/Event/Light/Camera coverage
WotLK nested track semantics
external .anim payload selection
Vanilla float C4Vector quaternion evidence
```

不能替代 Golden：不负责 `.skin->embedded View`、AnimationLookup、Playable226、canonical 324B v256 header、DBC、MPQ。根目录未观察到 LICENSE；本项目不复制其脚本，只独立实现格式事实。

## 当前 C++ M2 模块

```text
WotlkM2Reader          full v264 fixed header refs
AnimationMetadata
LegacyTrack
ExternalAnim
QuaternionCodec
BoneWriter
AuxiliaryTrackWriters  Color/Transparency/TexAnim/Attachment
CameraWriter
EventWriter
LightWriter            REFERENCE_GATED
ClassicM2Header
SkinViewWriter
RibbonWriter           external .anim integrated
ParticleWriter V2      external .anim integrated
ClassicM2Validator
ClassicM2Writer        whole-M2 canonical assembler
```

## Whole-M2 canonical assembler

新增：

```text
include/turtle335/m2/ClassicM2Writer.h
src/m2/ClassicM2Writer.cpp
tests/test_classic_m2_writer.cpp
```

当前组装路径：

```text
v264 source
+ ordered .skin files
+ build12340 AnimationData.dbc
+ optional per-sequence .anim bytes
        ↓
324B MD20 v256 header
name + terminating NUL
GlobalSequences
Sequence68
AnimationLookup
Playable226
Bone108
KeyBoneLookup
Vertex48
embedded View44
Color56
Texture defs + relocated filename strings
Transparency28
TextureAnimation84
TextureReplace / RenderFlags / lookup arrays
BoundingTriangles / Vertices / Normals
Attachment48
Event44
Camera124
Ribbon220
Particle504
Light212 (production default blocked by gate)
        ↓
ClassicM2Validator
```

未知/lossy semantic class fail closed，不会静默 strip。

## External `.anim`

```text
sequence.flags & 0x20 != 0 -> inner payload from main M2
sequence.flags & 0x20 == 0 -> inner payload from <Stem><AnimID:04>-<SubID:02>.anim
```

Outer Times/Keys refs 与 inner count/offset pair 仍从主 M2 读取；inner payload 才切换数据源。Bone/Color/TexAnim/Attachment/Event/Camera/Ribbon/Particle 均可走 resolver。

## Legacy one-key nuance

成功112历史 corpus 对部分普通 value track 同时出现过：

```text
ConstantNoRanges
PerSequence start/end expansion
```

两种历史 writer 形式都出现过，不能宣称其中一种是全格式唯一表示。代码保留显式 `LegacySingleKeyPolicy`；canonical writer 做确定性选择，真实 Turtle/112 Golden 是最终 oracle。

Ribbon/Particle 的 Golden 特例继续使用 `PerSequence`。

Attachment source enabled outer=0 的直接 Golden 规则：

```text
Ranges=[]
Times=[0]
Keys=[1]
```

已实现并加入 regression。

## Quaternion / Bone

```text
WotLK quaternion short4 8B -> Classic float4 16B
spline                  24B -> 48B
source -1 component         -> exact +1.0f
Bone                     88B -> 108B
```

Bone empty scaling 使用 identity `(1,1,1)`。

## Camera / Event / Light

Camera100->124：V4.4 selected paired Golden 25/25 fixed + track semantic match。Camera transpos/transtar 物理 key 为历史 `BigFloat` 36B (`Vec3[3]`)。

Event36->44：timer 为特殊 no-key track；只产生 Ranges/Times，空 sequence 不制造伪事件，并支持 external `.anim` time payload。

Light156->212：Wallcraft+Coffee 独立结构交叉确认并已实现，但 selected paired Golden 尚无 non-zero Light，因此 `ClassicM2Writer` 默认阻止 Light-bearing model 进入 production。

已新增定向采样器：

```text
tools/modelport/Collect_Light_Golden_V46.py
tools/modelport/Run_Collect_Light_Golden_V46.ps1
```

只读 M2 header，最多打包5个 source/target 都 `Light>0` 的同路径 pair；不做全库深扫。

## Ribbon / Particle

Ribbon：176B->220B，420+ emitters / 2520 tracks Golden offline；external `.anim` 已接入。

Particle V2：476B->504B，selected Golden 772 emitters；external `.anim` 已接入；`nUnknownReference != 0` 继续 hard BLOCK。

## Header / View / Validator

```text
Classic/Turtle header       324B MD20 v256
.skin header                48B -> embedded View44B
WotLK submesh               48B -> Classic32B
TextureUnit                 24B copy
```

Strict validator 已存在；whole writer 默认在返回前运行 validator。

## 下一步（不广扫）

```text
CMake/CTest validation of current whole writer
 -> whole writer vs selected static/animated/Ribbon/Particle Golden families
 -> Light targeted Golden（若找到）
 -> strengthen validator for newly integrated blocks
 -> minimal Turtle 1.18.1 Ribbon + Particle/Creature real-client regression
 -> M2 production lock
```

M2 锁定后再继续 WMO / ADT-WDT-WDL / DBC / MPQ 统一流水线。
