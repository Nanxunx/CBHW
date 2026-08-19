# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 19:57 +08:00（V4.6 canonical-226 + whole-M2 assembly）

权威仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

后续继续工作优先读取：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1957.md
docs/research/WALLCRAFT_M2_WORKSHOP_ANALYSIS_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V46_REFINE_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_WRITER_CORRECTION_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_V45_RIBBON_2026-08-19.md
```

## 最终目标

把 WoW 3.3.5a Build12340 的 M2/WMO/地图/BLP/DBC 等资源语义降版成 Vanilla 1.12.x / Turtle WoW 1.18.1 标准资源，并最终自动构建 Patch MPQ。M2 输出只能是标准 `MD20 v256`，不依赖 Orange、010 Editor 或其他 private runtime。

## Canonical animation baseline

- `AnimationLookup`: **PRODUCTION_BASELINE**。
- `PlayableAnimationLookup`: **PRODUCTION_BASELINE_V46_CANONICAL_226**。
- canonical overrides：`28->27, 108->111, 112->111, 121->14, 146->0, 172->16, 174->16, 181->19, 191->159`。
- requested ID 自身存在时优先自身，否则 model-aware recursive fallback。
- `Sequence.Index` 保留 source。
- canonical Sequence metadata source-preserving；timeline 每 sequence 前 `+3333`，随后加 source duration。
- V4.5 refinement 已把旧498 Playable mismatch 收敛；27 个非canonical target 使用 Playable203/1，不作为 writer 目标。

## Wallcraft M2 Workshop 结论

`Wall-core/M2Workshop` 是 010 Editor 的跨 Vanilla/TBC/WotLK **同版本 M2 编辑脚本**，不是 3.3.5a -> Vanilla/Turtle retroport converter。

它对本项目最有价值的是：

```text
固定结构/offset 表交叉验证
Bone/Color/Transparency/TexAnim/Attachment/Event/Light/Camera 覆盖
WotLK nested track 结构
external .anim sidecar 自动解析逻辑
Vanilla float C4Vector quaternion 独立证据
```

它不处理 `.skin -> embedded View`、AnimationLookup、Playable226、324B v256 header、DBC/MPQ，因此不能替代真实112/Turtle Golden。根目录未观察到 LICENSE；本项目不复制其脚本，只独立实现格式事实。

## 当前 C++ M2 模块

```text
WotlkM2Reader            （完整304B v264 header refs 已暴露）
AnimationMetadata
LegacyTrack
ExternalAnim
QuaternionCodec
BoneWriter
AuxiliaryTrackWriters    （Color/Transparency/TexAnim/Attachment）
CameraWriter
EventWriter
LightWriter              （REFERENCE_GATED）
ClassicM2Header
SkinViewWriter
RibbonWriter
ParticleWriter
ClassicM2Validator
```

### External `.anim`

已实现：

```text
sequence.flags & 0x20 != 0 -> inner payload from main M2
sequence.flags & 0x20 == 0 -> inner payload from <Stem><AnimID:04>-<SubID:02>.anim
```

outer Times/Keys refs 与 inner count/offset pair 仍从主 M2 读取；只有 inner payload 数据源切换。Ribbon writer 本轮也已接入此 resolver；Particle V2 的 external-track 接入仍待完成。

### Legacy track representation — 重要修正

成功的历史112 paired corpus 对**普通 generic 一键轨**存在两种可运行写法：

```text
A. ConstantNoRanges
B. PerSequence start/end expansion
```

因此不能说 Bone/Color/etc 只有一种客户端合法表示。当前 writer 把两种策略显式化；选择一个确定性的 canonical 输出，但真实 Turtle/112 Golden 仍高于历史 converter 风格。

Ribbon/Particle 的 Golden 一序列特例继续使用 `PerSequence`。

Attachment 另有独立 Golden 规则：当 source enabled track outer count=0 时，成功 target 使用：

```text
Ranges=[]
Times=[0]
Keys=[1]
```

本轮已修正 `ConvertWotlkAttachments` 并加入 regression。

### Quaternion / Bone

```text
WotLK short4     8B  -> Classic float4 16B
spline           24B -> 48B
source -1 component -> exact +1.0f
Bone 88B -> 108B
```

Bone empty scaling 使用 identity `(1,1,1)`，不是 zero scale。

### Camera

V4.4 selected paired Golden 已得到 25/25 Camera records 的 fixed fields + 3 tracks semantic match；本轮已加入 `CameraWriter`：

```text
WotLK Camera100 -> Classic Camera124
transpos  +16 -> +16
position  +36 -> +44
transtar  +48 -> +56
target    +68 -> +84
roll      +80 -> +96
```

Camera transpos/transtar 的物理 key 是历史 `BigFloat` 36B (`Vec3[3]`)。

### Event

已加入 `EventWriter`：

```text
WotLK Event36 -> Classic Event44
```

Event timer 是特殊 **no-key** track：只生成 Ranges/Times，空 sequence 不制造伪事件。支持 external `.anim` time payload。

### Light

已加入 `LightWriter` 的结构实现：

```text
WotLK Light156 -> Classic Light212
```

结构由 Wallcraft + Coffee 独立实现交叉确认，但目前我们打包的 selected Golden 中没有 non-zero Light pair，因此状态为：

```text
REFERENCE_GATED
```

在 whole writer 默认生产路径解锁 Light 前，最好取得一个非常小的成功335/112 Light Golden pair，不需要重新广扫全库。

### Ribbon / Particle

Ribbon：176B -> 220B；420+ emitters / 2520 tracks Golden offline；现已能解析 external `.anim`。

Particle V2：476B -> 504B；selected Golden 772 emitters；`nUnknownReference != 0` 继续 hard BLOCK。external `.anim` resolver 还要接进 Particle tracks。

### Header / View / Validator

```text
Classic/Turtle MD20 v256 header = 324B
.skin 48B header -> embedded View44B
WotLK submesh48 -> Classic32
TextureUnit24 copy
```

Strict validator 已存在；输出只有验证通过才进入最终 patch pipeline。

## 下一步（不要广扫）

直接执行：

```text
Particle V2 external .anim integration
        ↓
whole-M2 relocation assembly
        ↓
textures/lookup/static arrays relocation
        ↓
MD20 v256
        ↓
ClassicM2Validator
        ↓
最少量 Turtle 1.18.1 实机 regression
```

当前不需要用户重新提供普通模型或重新扫描。唯一可能追加的数据请求是 **1个带 non-zero Light 的成功335/112 paired Golden family**；如果现有完整 paired corpus可在用户本机自动定向提取，则只提这一类，不再做全库重复打包。

ADT/WDT/WDL、WMO、完整 DBC/MPQ 自动化仍属于后续统一流水线阶段；不能把当前 M2 进度误称为所有地图/建筑均完成。
