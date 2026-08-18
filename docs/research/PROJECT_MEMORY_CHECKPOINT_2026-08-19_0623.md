# 模型移植项目 — Golden Reference 第二批 V4 权威检查点

更新时间：2026-08-19 06:23 +08:00

权威源码仓库：`NansenCore/Turtle335Converter`，分支 `main`。

## 数据基线

```text
335 Raw Complete:
E:\335_FinalExtract_V5

成功降版 1.12 Golden:
E:\335to112_Converted_FinalExtract_V1
```

已知精确同路径 M2 pairs：约 11,597。

第二批输入 `ModelPort_GoldenReference_SecondBatch_ALL.zip`：13组核心 M2 pair，Manifest 184 / OK150 / optional missing34 / required missing0。

## V4 最新裁决

### Sequence.Index

V3 的 `Index=physical position` 已废弃。

正确：

```text
Sequence.Index = preserve source Index / aliasNext
```

DeathCoil、Sunwell、FelReaver 均有成功目标 `Index != position`。

### AnimationLookup

```text
count = max(AnimationID)+1
missing = 0xFFFF
```

重复 AnimationID：优先 `SubAnimationID==0`，没有 sub0 才取第一 physical occurrence。

不能复制 WotLK source AnimationLookup；FelReaver source 37项而成功 target 重建为191项。

### PlayableAnimationLookup

固定 226×4B。

重要纠正：**不再直接以 build12340 AnimationData.dbc field5 作为完整 fallback graph。**

当前 Golden V4 graph = historical 226-era fallback graph + 成功1.12 target直接观察扩展：

```text
170->16
171->16
172->16
173->16
174->16
175->30
176->16
178->16
179->16
181->16
191->159
```

加入这些边后，对第二批 13/13 成功 target 的全部226 records 逐项 exact match。

fallback flags：

```text
6/97/100/115/123/132/188 -> 3
13/45/101/189            -> 1
others                    -> 0
```

### Sequence timeline

13/13：每条 sequence 前 `+3333ms`，然后 `end=start+source.length`。

## 已扩展验证的低风险部分

### Embedded View

第二批 17 views：source skin -> target embedded View 的 indices/triangles/properties/TextureUnit exact，Classic submesh = source 48B LKSubmesh 前32B。17/17 PASS。

### 普通 BLP

39 对同路径普通3D BLP：39/39 SHA256 exact。继续原样保留；UI icon/cursor 单独处理。

### External .anim

FelReaver0069-00.anim、Muru0060-00.anim：335/112 SHA256 exact。当前 validated policy = 原样复制。

### TextureAnimation

10 records。GlobalSequence track 保持 `Ranges=0` + raw times/keys；VR_Elevator per-animation track 再次验证 generic Legacy Range/Times/Keys。

## Particle 当前进度

第二批 7 模型 / 50 emitters：

```text
source stride=476
target stride=504
emitter count preserved
target flags = source flags & 0xFFFF
```

50/50 raw prefix 观察：

```text
source byte +0x29 = 1/2
target byte +0x29 = 0
target uint16 +0x2A == source byte +0x29
```

3-key Fake color/alpha/size 的 Classic fixed-gradient 降级已获得强证据；DeathCoil 2-key case 会真实 lossy 清零固定 block。

Wand 单 Sequence particle track 存在 `source single key -> target start/end 2 keys + ranges` 特殊分支，FULL Particle 仍未 BATCH_READY。

## Ribbon

第二批文件名带 Ribbon 的模型真实 `nRibbonEmitters=0`。第二批13组全部 ribbons0。

Ribbon 仍未 Golden 验证；下一批必须直接按 Header `nRibbonEmitters>0` 全库筛选。

## 当前源码修正

`tools/modelport/playable_lookup_v4.py`：改为 Golden-validated fallback graph，不再由 build12340 DBC field5 直接生成。

`tools/modelport/animation_metadata_v4.py`：
- preserve source Sequence.Index
- AnimationLookup prefer sub0
- 3333 timeline
- 226 Playable V4
- quaternion -1 exact +1.0

`tools/modelport/validate_animation_metadata_v4.py`：不再要求 AnimationData.dbc 参数，直接验证 Golden V4 graph。

## 下一步：第三批全库 AutoScan

运行自动扫描脚本：

```text
Pack_ModelPort_GoldenReference_ThirdBatch_AutoScan.ps1
```

对 `E:\335_FinalExtract_V5` 中所有存在成功112 counterpart 的 M2 全库验证：

1. Sequence metadata source/target
2. AnimationLookup V4
3. Playable V4 226
4. 真 RibbonEmitter
5. Particle 单动画/复杂
6. Alias/SubAnimation
7. external .anim

自动打包优先级：V4 mismatch -> True Ribbon -> Particle one-animation -> Particle complex -> Alias/SubAnimation。

如果全库 PlayableV4 mismatch=0，Playable graph 才升级为 production baseline；然后只需极少实机 Golden 回归，不进行逐模型客户端试错。
