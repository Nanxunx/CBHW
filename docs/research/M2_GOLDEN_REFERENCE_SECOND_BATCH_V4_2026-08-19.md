# M2 Golden Reference 第二批 V4 — 3.3.5a -> 1.12 成功成品反推

日期：2026-08-19 06:23 +08:00

输入：`ModelPort_GoldenReference_SecondBatch_ALL.zip`

第二批共 13 对同路径 `335 v264 / 成功 1.12 v256` 核心 M2，Manifest 184 条：150 OK / 34 optional-missing / 0 required-missing。

## 1. Sequence.Index：V3 规则正式废弃

第二批证明：

```text
Sequence.Index != physical sequence position
```

成功目标保留 WotLK source 的 Index/aliasNext 语义。

例：

```text
DeathCoil
position0 AnimID0   Index1
position1 AnimID144 Index0

Sunwell
AnimID0   Index1
AnimID158 Index0
AnimID159 Index2
```

FelReaver 也存在大量 `Index != position`。

13/13 成功目标的 Sequence 语义字段与 source 一致：

```text
AnimID / SubAnimID / duration / moveSpeed / flags / probability
unused / d1 / d2 / playSpeed / nextAnimation / Index
```

正确规则：

```text
Target Sequence.Index = preserve source Index
```

旧 target 如果已经把 Index 清零，repair 必须重新读取 original v264 source；不能从损坏 target 猜回 alias 语义。

## 2. Sequence timeline

13/13 成功目标均采用：

```text
timeline = 0
for each sequence:
    timeline += 3333
    start = timeline
    end = start + source.length
```

## 3. AnimationLookup

13/13 成功目标均满足：

```text
count = max(AnimationID)+1
missing = -1 / 0xFFFF
```

重复 AnimationID 时：

1. 优先 `SubAnimationID == 0`
2. 若该 ID 没有 sub0，则选择第一 physical occurrence

FelReaver source 自己已有 37-entry WotLK lookup，但成功 target 仍重建为 191-entry，因此禁止直接复制 source lookup。

## 4. PlayableAnimationLookup V4

所有成功 target：

```text
226 records
record = int16 RealAnimationID + int16 Flags
```

历史 226-era LKBC fallback graph 单独即可精确复现第二批 11/13 模型全部226项。

从成功 target 直接得到下列扩展边：

```text
170 -> 16
171 -> 16
172 -> 16
173 -> 16
174 -> 16
175 -> 30
176 -> 16
178 -> 16
179 -> 16
181 -> 16
191 -> 159
```

加入扩展后：

**V4 generator 对第二批 13/13 成功 target、全部 226 records 逐项 exact match。**

fallback flags：

```text
6/97/100/115/123/132/188 -> 3
13/45/101/189            -> 1
other fallback            -> 0
```

重要纠正：不要再把 build12340 `AnimationData.dbc` field5 直接当成完整 Playable fallback graph。该路线对部分模型有效，但不能复现成功 FelReaver/Sunwell target；当前权威规则是 Golden-validated graph。

## 5. External .anim

第二批实际取得：

```text
FelReaver0069-00.anim
Muru0060-00.anim
```

两组均：

```text
335 size == target size
335 SHA256 == target SHA256
```

当前 policy：已验证 external `.anim` payload 原样复制。

## 6. Embedded View / SKIN

第二批共验证 17 个真实 source skin view。

对应 target embedded View：

```text
counts exact
LOD exact
indices exact
triangles exact
properties exact
TextureUnit exact
Classic submesh == source 48-byte LKSubmesh 前32字节
```

17/17 PASS。

因此 converter 必须遍历全部 source skin profiles，不得限制 `nViews==1`。

## 7. 普通 3D BLP

第二批可同路径双侧比较 39 对普通 BLP：

```text
39/39 SHA256 exact
```

结论继续：普通兼容 3D BLP 原样保留；UI Icon/cursor 是独立安全转换路径。

## 8. TextureAnimation

第二批共 10 个：Auchindoun 3 / Sunwell 6 / VR_Elevator 1。

GlobalSequence tracks：

```text
Ranges = 0
Times raw preserved
Keys raw preserved
```

VR_Elevator per-animation translation source groups：

```text
3,69,33,70,0
```

成功 target：

```text
Ranges = 6
Times = 177
Keys = 177
```

空 source animation 自动合成 start/end 两 timestamp + 两个 zero Vec3，完全符合当前 generic Legacy Range/Times/Keys 规则。

## 9. Particle — 50 emitter Golden evidence

第二批 7 个模型共 50 emitters。

全部确认：

```text
source stride = 476
target stride = 504
emitter count preserved
target flags == source flags & 0xFFFF
```

Raw prefix 观察（先记录 offset 事实，不强行套历史字段名）：

```text
source byte +0x29 = 1/2
target byte +0x29 = 0
target uint16 +0x2A == source byte +0x29
```

50/50 成立。

Fake gradient 成功降级规律：source color 恰好 3 keys 时：

```text
midPoint = color.times[1] / 32767.0
RGB float -> target B,G,R
alpha8 = source opacity key >> 7
3-key size -> target 3 float sizes
```

DeathCoil 中 2-key color / 2-key size 的 emitter，成功 target 可把 legacy 固定块置零，属于真实 lossy downgrade；某些 source unknown3/unknown4 非零内容也被目标清零。

普通 particle float track 多数单key常量为：

```text
Ranges=0 / Times=1 / Keys=1
```

但 Wand（单 Sequence）出现 `single key -> start/end 两key + ranges` 的特殊分支，所以 FULL Particle writer 仍需第三批补充单动画样本。

## 10. Ribbon 尚未 Golden 验证

`Auchindoun_Ethereal_Ribbon_Type3.m2` 虽然名字带 Ribbon，但真实 Header：

```text
nRibbonEmitters = 0
```

第二批 13 组全部 `ribbons=0`。

下一批必须直接扫描整个 335 库 Header 的 `nRibbonEmitters>0`，禁止再按文件名猜。

## 11. Header count preservation

第二批 13/13 pairs 的普通 section count 均 source->target 保留，包括：

```text
global sequences / animations / bones / key-bone lookup / vertices
colors / textures / transparency / texanims / render flags
lookups / bounds / attachments / events / lights / cameras
ribbons / particles
```

`source nViews == target embedded View count`。

Target 额外重建的是 AnimationLookup 和 PlayableAnimationLookup。

## 12. Name length

第二批成功 target 全部表现为：

```text
target name count = source name count + 1
```

即 Classic target name count 包含结尾 NUL。

## 13. V4 validator 状态

新的 V4 validator：

- 第二批 13/13 成功 target PASS
- 本项目旧 Crossbow AnimationV1 FAIL

旧失败版可准确抓到：

```text
AnimationLookup mismatch
Playable[160]/[161] mismatch
source Sequence.Index 1/2 被错误写成0
```

## 14. 下一步：第三批全库 AutoScan

第三批不再人工猜模型。

扫描：

```text
<BUILD12340_SOURCE_ROOT>
        ↕ exact path
<CLASSIC_GOLDEN_ROOT>
```

对所有 paired M2 自动验证：

1. source/target Sequence metadata
2. AnimationLookup V4
3. 226 Playable V4
4. true RibbonEmitter count
5. Particle / TexAnim / Alias / SubAnimation / external .anim 特征

自动优先打包：

1. 任意 V4 rule mismatch
2. 真正 `RibbonEmitters>0`
3. Particle + 单动画
4. Particle复杂模型
5. Alias/SubAnimation 高覆盖模型

如果全库 `Playable V4 mismatch = 0`，Playable V4 graph 才提升为 production baseline。
