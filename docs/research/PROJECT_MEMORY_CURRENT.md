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

## V4.4 / V4.6 corpus

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

V4.6 新增：

```text
28  -> 27
108 -> 111
112 -> 111
```

三条解释 18/18 remaining canonical models、20/20 records。

## Canonical animation baseline

- `AnimationLookup`: **PRODUCTION_BASELINE**。
- `PlayableAnimationLookup`: **PRODUCTION_BASELINE_V46_CANONICAL_226**。
- canonical overrides：`28->27, 108->111, 112->111, 121->14, 146->0, 172->16, 174->16, 181->19, 191->159`。
- requested ID 自己存在时优先自身；否则 model-aware recursive fallback。
- `Sequence.Index` 保留 source。
- Sequence metadata canonical 路径 source-preserving；timeline 使用每 sequence 前 `+3333`，随后加 source duration。
- 29 个历史 Sequence/Timeline exceptions：27 个属于旧 lossy/noncanonical output；2 个 canonical226 仅 bounds 不同。因此不复制旧转换器清 flags/Index/改 timeline 的行为。

## 当前 C++ M2 模块

```text
WotlkM2Reader
AnimationMetadata
LegacyTrack
ExternalAnim
QuaternionCodec
BoneWriter
ClassicM2Header
SkinViewWriter
RibbonWriter
ParticleWriter
ClassicM2Validator
```

### Classic header / View

```text
Classic/Turtle header = MD20 v256 / 324 bytes
.skin 48B header -> embedded View 44B
48B WotLK submesh -> 32B Classic submesh
24B TextureUnit copy
```

### External `.anim`

Wallcraft M2 Workshop 提供了重要独立证据，本轮已经实现 sidecar resolver：

```text
sequence.flags & 0x20 != 0 -> payload from main M2
sequence.flags & 0x20 == 0 -> payload from <Stem><AnimID:04>-<SubID:02>.anim
```

Outer nested refs 仍来自主 M2；inner payload offset 根据 sequence 选择主 M2 或 sidecar。

Wallcraft 是 010 Editor 同版本编辑脚本，不是 retroport converter；只作为交叉证据，不成为工具依赖。

### Legacy track policy

不能把 Ribbon/Particle 的 one-sequence 特例推广给全部 block。

现在区分：

```text
PerSequence
    Ribbon / Particle

ConstantNoRanges
    Bone / Color / Transparency / TexAnim /
    Attachment / Light / Camera
```

### Quaternion

```text
WotLK compressed key : 4 * int16 = 8B
Classic/Turtle key   : 4 * float = 16B
spline               : 24B -> 48B
source -1 component  : exact +1.0f
```

用户 V4.5 refinement paired data 上已经观察到 **1222 个 compressed quaternion -> float4 exact target matches**；缺少的部分主要是精简包未携带实际引用的完整 source .anim family。

### Bone writer

已接入：

```text
WotLK Bone 88B -> Classic/Turtle Bone 108B
```

规则：

```text
source +0..11   -> target +0..11
source +12 unk  -> drop
translation +16 -> target +12
rotation    +36 -> target +40, compressed quaternion -> float4
scaling     +56 -> target +68
pivot       +76 -> target +96
```

Bone empty scaling 使用 `(1,1,1)` identity。用户 refinement Golden 中存在能区分 zero/identity 的实际样本，identity 匹配成功 target。

### Ribbon / Particle

Ribbon：C++ writer 已接入；Golden offline 420+ emitters / 2520 tracks 级别验证。

Particle V2：C++ writer 已接入；target record 504B；`nUnknownReference != 0` 继续 hard BLOCK，直到拿到非零 Golden。

### Strict validator

Classic/Turtle v256 validator 已存在，只有通过验证的 output 才进入最终 patch pipeline。

## 下一步（不要广扫）

直接继续：

```text
Color / Transparency writer
TextureAnimation writer
Attachment writer
Event writer（特殊 no-key timer）
Light writer
Camera writer
        ↓
whole-M2 relocation assembly
        ↓
MD20 v256
        ↓
ClassicM2Validator
        ↓
最少量 Turtle 1.18.1 实机 regression
```

当前不需要用户再次提供普通模型或重新扫描。需要额外数据时，只定向请求具体缺口，不再做整库重复工作。

ADT/WDT/WDL、WMO、完整 DBC/MPQ 自动化继续属于后续统一流水线阶段；当前不能把 M2 进度误称为所有地图/建筑已经完成。
