# M2 Golden Reference V3 — 2026-08-19

状态：**Reference-aligned / awaiting one real-client animated Golden test**

## 目标

把 WoW 3.3.5a build 12340 `MD20 v264` 安全降级为 Vanilla/Turtle 可加载的标准 `MD20 v256`。不使用 `.orange`、不写私有 Header、不依赖 OrangeM2/OrangeConverter DLL。

## 本轮证据

现在拥有同一模型的三方对照：

1. 3.3.5a 原始 `Bow_2H_Crossbow_PVP330_D_01`，v264，82496 bytes；
2. 别人已经成功降到 1.12 并实际可用的同模型，v256，99056 bytes；
3. 本项目此前生成、会触发 `ERROR #132` 的 AnimationV1，v256，97459 bytes。

同时保留已实机通过的：

- `Sword_1H_PVP330_D_01` / DisplayID 65385；
- `Mace_1H_PVP330_D_02` / DisplayID 65372。

## Crossbow #132 已显著收敛的关键差异

### 1. Classic AnimationLookup 必须重建

成功 1.12：

```text
animations = 3
AnimationID = 0, 160, 161
AnimationLookup.count = 162
lookup[0]   = 0
lookup[160] = 1
lookup[161] = 2
其它        = 0xFFFF
```

失败 AnimationV1：

```text
AnimationLookup.count = 0
AnimationLookup.offset = 0
```

当前规则：

```text
count = max(AnimationID) + 1
missing = 0xFFFF
lookup[AnimationID] = SequenceIndex
```

### 2. Sequence Index 不可全部写 0

成功成品与 335 源语义一致：

```text
Anim 0   -> Index 0
Anim 160 -> Index 1
Anim 161 -> Index 2
```

失败 AnimationV1 为 `0,0,0`。

### 3. PlayableAnimationLookup 为 226 项，但必须 model-aware

Sword / Mace / Crossbow 成功样本均确认：

```text
PlayableAnimationLookup.count = 226
record size = 4
```

Crossbow 成功版：

```text
playable[160] = (160, 0)
playable[161] = (161, 0)
```

失败 AnimationV1 对应位置是 `(0,0)`。

因此不能把 226 项完全当作一份静态常量原样复制；基础 fallback 可复用，但当前模型实际拥有的 AnimationID 需要更新对应项。

### 4. Quaternion endpoint

compressed `int16(-1)`：

```text
失败：0.999969482421875
成功：1.0
```

正式规则：`-1 -> exact +1.0f`。

## 已被成功成品验证为正确的部分

失败 AnimationV1 与成功 1.12 Crossbow 对照显示：

- Bone2~5 Legacy Range 完全一致；
- Times 完全一致；
- Keys 数量完全一致；
- 非 `-1` Quaternion component 一致；
- external `.skin` -> Classic embedded View 的 indices / triangles / properties 一致；
- Classic submesh payload 一致；
- TextureUnit payload 一致；
- Attachment 语义一致；
- Event legacy ranges 一致。

因此以下旧怀疑已降级：

- Legacy Range 算法；
- Times timeline；
- Keys count；
- embedded View；
- Classic Submesh；
- Event legacy track；
- 强制 16-byte alignment。

成功 Crossbow 本身多个关键 section offset 并未 16-byte 对齐，因此 universal 16-byte alignment 不是目标必要条件。

## Particle / Texture

335 Crossbow：`textures=3, particles=2`。

成功 1.12：`textures=3, particles=2`。

旧 SAFE AnimationV1：`textures=2, particles=0`。

Particle 缺失不是当前 #132 的首要原因，但 FULL 模式后续应按成功参考保留/降级 Particle。

## BLP

Crossbow / Sword / Mace 的 3D 主纹理在 335 与成功 1.12 中 SHA256 完全相同，说明兼容 3D BLP 可原样保留。

UI Icon 单独转换：

```text
335: BLP2 encoding=2 alphaDepth=8 alphaEncoding=7 mip=0x11
112: BLP2 encoding=1 alphaDepth=0 alphaEncoding=8 mip=0x01
```

本项目此前 DXT1 CursorSafe 路线也已在真实 Turtle 客户端通过，因此 UI Icon 至少存在两条安全路线。

## ItemDisplayInfo.dbc

成功成品再次确认：

```text
WotLK: 25 fields / 100-byte record
Classic: 23 fields / 92-byte record
```

DisplayID 65337 / 65372 / 65385 均保留原 ID，字段映射与当前项目 25 -> 23 规则一致。

## 下一步

Animated Converter 先实现并锁定：

1. Sequence Index；
2. AnimationLookup；
3. model-aware 226 PlayableAnimationLookup；
4. exact quaternion `-1 -> +1.0`；
5. 继续使用已经验证正确的 Legacy Range / Times / Keys；
6. 继续使用已经验证正确的 embedded View。

离线 Validator 通过后，只做一次新的 Animated Golden real-client test，再决定是否把 Active Bone Animation 提升为 Batch Ready。

下一批 Golden Reference 将覆盖：

- 更多 Crossbow / Rifle / Wand；
- Particle；
- Ribbon；
- Spell FX / TexAnim；
- Active Doodad；
- external `.anim`。
