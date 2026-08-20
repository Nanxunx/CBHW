# 模型移植项目 — 最新记忆检查点（2026-08-19 05:18 +08:00）

本文件记录 `NansenCore/Turtle335Converter` 当前最新工程状态。若与 `PROJECT_MEMORY_CURRENT.md` 或 `PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md` 在本轮新增 M2 结论上冲突，以本文件为准；旧文件中未冲突的 ADT/WMO/DBC 长期结论继续有效。

## 用户持续指令

- 本项目是长期“模型移植”项目。
- 用户要求继续时直接推进，不重复询问已经确认的目标。
- 优先使用真实二进制、真实客户端、成功成品与源码证据。
- OrangeM2/OrangeConverter 只作为算法参考，不允许把 Orange 私有格式、Header 或 DLL 依赖带入最终输出。
- 最终目标仍是标准 `MD20 v256`、Classic/Turtle DBC、标准 BLP、正常 MPQ staging。

## 当前数据基线

### 3.3.5a 原始完整研究库

```text
<BUILD12340_SOURCE_ROOT>
```

14 个 MPQ 已严格串行提取完成。

代表数量：

```text
Weapon: 1379 M2 / 1394 skin / 4208 BLP
Creature: 951 M2 / 2080 skin / 4971 anim / 3612 BLP
Spells: 1567 M2 / 1654 skin / 44 anim
DBFilesClient: 246 DBC
Interface\Icons: 6308 BLP
```

### 已成功降到 1.12 的 Golden Reference 库

```text
<CLASSIC_GOLDEN_ROOT>
```

来自 `patch-3.mpq` 到 `patch-8.mpq`，按旧 -> 新顺序串行提取并以后包覆盖前包。

最终数量：

```text
12512 M2
6929 ANIM
68683 BLP
5481 WMO
5127 ADT
65 WDT
65 WDL
132 DBC
```

3.3.5 原始 M2 与成功 1.12 M2 直接同路径匹配：11597 对。

## 第一批 Golden Reference

第一批包括：

- Crossbow 65337
- Sword 65385
- Mace 65372
- ItemDisplayInfo.dbc
- AnimationData.dbc
- 完整索引/来源/SHA256

### Sword 65385

真实客户端 FULL PASS：几何、纹理、反射、朝向、尺寸、握持、图标均正常。

### Mace 65372

真实客户端 geometry PASS。此前不可见问题实际来自 DBC/MPQ overlay conflict，而不是 M2 几何。

### Crossbow 65337

335 原始：

```text
MD20 v264
82496 bytes
10 bones
3 animations
AnimationID = 0,160,161
```

别人成功 1.12：

```text
MD20 v256
99056 bytes
```

本项目旧 AnimationV1：

```text
MD20 v256
97459 bytes
ERROR #132 / ACCESS_VIOLATION
```

## Crossbow #132 当前最关键差异

### 1. AnimationLookup

成功：

```text
count = 162
lookup[0]   = 0
lookup[160] = 1
lookup[161] = 2
其余        = 0xFFFF
```

失败旧版：

```text
count = 0
```

当前正式规则：

```text
count = max(AnimationID)+1
missing = 0xFFFF
lookup[AnimationID] = SequenceIndex
```

### 2. Sequence Index

成功：`0,1,2`

失败旧版：`0,0,0`

规则：不得统一写 0；保留或重建真实 sequence index。

### 3. PlayableAnimationLookup

成功 Sword / Mace / Crossbow 与项目已通过 Sword 均确认：

```text
count = 226
record size = 4
```

成功 Crossbow：

```text
playable[160] = (160,0)
playable[161] = (161,0)
```

失败旧版对应位置为 `(0,0)`。

规则：226 项基础 fallback 可复用，但数组必须 model-aware，根据当前模型实际 AnimationID 更新对应项。

### 4. Quaternion

compressed short `-1`：

```text
失败旧版 -> 0.999969482421875
成功成品 -> exact 1.0
```

规则：`-1 -> exact +1.0f`。

## 已经验证正确的动画/几何部分

Crossbow 失败版与成功版逐字段对照：

- Bone2~5 Range 完全一致；
- Times 完全一致；
- Key count 完全一致；
- 非 -1 quaternion component 一致；
- embedded View indices/triangles/properties 一致；
- Classic submesh payload 一致；
- TextureUnit payload 一致；
- Attachment 语义一致；
- Event legacy ranges 一致。

因此以下旧怀疑已降级/排除：

- Legacy Range 算法；
- Times timeline；
- Keys count；
- external skin -> embedded View；
- Classic submesh；
- Event legacy rebuild；
- universal 16-byte alignment。

成功 Crossbow 的多个 section offset 本身不是 16-byte aligned，因此强制全局 16-byte 对齐不是必要条件。

## Animated Converter V3

已经建立 Golden-Reference-aligned helper 与 validator：

```text
tools/modelport/animation_metadata_v3.py
tools/modelport/validate_animation_metadata_v3.py
tools/modelport/target_policy.py
```

对旧 Crossbow AnimationV1 做 metadata repair 后，离线 Validator：

```text
PASS
```

并确认：

```text
Sequence 0/160/161 -> 与成功成品一致
AnimationLookup payload -> 与成功成品一致
226 Playable payload -> 与成功成品一致
Bone2~5 rotation Range/Times/Keys/Quaternion -> 与成功成品一致
```

仍未把这个 repair candidate 当作最终生产 converter；需要再用下一批 Golden Samples 证明规则可泛化，然后只做一次真实客户端 Animated Golden test。

## BLP 新规则

Crossbow / Sword / Mace 的 3D 主纹理在 335 与成功 1.12 中 SHA256 完全相同。

因此：兼容 3D BLP 直接保留即可。

UI Icon：

```text
335: BLP2 encoding2 alphaDepth8 alphaEncoding7 mip0x11
112: BLP2 encoding1 alphaDepth0 alphaEncoding8 mip0x01
```

项目自身 DXT1 CursorSafe 路线也已在真实 Turtle 客户端通过。

因此 UI Icon 安全路线至少有：

1. DXT1 CursorSafe；
2. 成功降版包的 paletted/no-alpha BLP2。

## DBC

ItemDisplayInfo：

```text
335: 25 fields / 100-byte records
112: 23 fields / 92-byte records
```

DisplayID 65337 / 65372 / 65385 保留原 ID，25 -> 23 字段映射得到成功成品独立验证。

AnimationData：

```text
335: 506 records / 8 fields / 32-byte records
112: 506 records / 7 fields / 28-byte records
```

后续继续用于 Playable/fallback 语义研究。

## 下一批 Golden Reference

自动打包脚本：

```text
tools/modelport/Pack_ModelPort_GoldenReference_SecondBatch.ps1
```

目标 13 个代表模型：

```text
Crossbow_PVP320
Crossbow_IcecrownRaid
Rifle_IcecrownRaid
Wand_IcecrownRaid_D02
Sunwell_BeamFX
DeathCoil_Missile
Auchindoun_Ribbon_Type3
ArcaneLightning
AO_Windmill
VR_Elevator_Lift
AGS_Engines
FelReaver
Muru
```

覆盖：

- AnimationLookup/Playable 泛化；
- Particle；
- Ribbon；
- Spell FX/TexAnim；
- Active Doodad；
- external `.anim`。

## 当前源码仓库

权威仓库：

```text
https://github.com/NansenCore/Turtle335Converter
branch: main
```

注意：此前误把同步目标指向 `1825679767/WYTurtle`，该判断已纠正。模型转换器源码与研究记忆应同步到 `NansenCore/Turtle335Converter`。

## 下一步执行顺序

1. 获取第二批 Golden Reference 总包；
2. 批量对照更多 animated models；
3. 固化 AnimationLookup / Sequence / Playable 泛化规则；
4. 分析 external `.anim`；
5. 分析 Particle / Ribbon / TexAnim；
6. Animated Converter 离线 validator 全绿；
7. 只做一次 Crossbow real-client Animated Golden test；
8. 若通过，将 Active Bone Animation 提升为 `BATCH_READY`；
9. 再推进 Particle / Ribbon FULL mode。
