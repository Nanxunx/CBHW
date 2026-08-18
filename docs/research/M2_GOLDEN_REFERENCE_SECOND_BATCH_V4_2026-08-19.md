# M2 Golden Reference 第二批 V4 — 3.3.5a -> 1.12 成功成品反推

日期：2026-08-19 05:34 +08:00

第二批共 13 对同路径 335 v264 / 成功 1.12 v256；与第一批 Sword/Mace/Crossbow 合计 16 个直接 Golden pairs。

## 1. Sequence.Index 正式纠错

第二批 13/13 模型的每一条 Sequence 都确认：AnimationID、SubAnimationID、moveSpeed、flags、probability、unused、d1/d2/playSpeed、bounds/radius、NextAnimation、**Index** 均保留 source 语义；仅 source length 转换成 Classic start/end timeline。

V3 曾推断 `Index == physical sequence index`，现在被否定。

FelReaver 直接证据：

```text
physical 3: AnimID 53, source Index 4 -> target Index 4
physical 4: AnimID 54, source Index 3 -> target Index 3
physical 9: AnimID 20, source Index10 -> target Index10
physical10: AnimID 30, source Index 9 -> target Index 9
```

正确规则：

```text
Sequence.Index = preserve source Index
AnimationLookup[AnimationID] = first physical sequence index
```

两者独立。

## 2. AnimationLookup 泛化

13/13 成功目标均满足：

```text
count = max(AnimationID)+1
missing = 0xFFFF
lookup[AnimID] = first physical sequence index with that AnimID
```

重复 AnimID 示例：FelReaver ID16 有 sub0/sub1，lookup[16]=1；FelReaver ID0 在 physical5/11，lookup[0]=5；Muru ID0 在 physical0/1，lookup[0]=0。

## 3. PlayableAnimationLookup

成功目标固定 226 records，每项 `int16 RealAnimationID + int16 Flags`。

V4 生成方式：

1. 读取 build12340 `AnimationData.dbc`。
2. 使用 8-field source schema 的 field5 `Fallback`。
3. 对 requested ID 0..225 递归 fallback，直到命中当前模型实际拥有的 AnimationID。
4. Golden override：`172->16, 174->16, 181->16, 191->159`。
5. fallback 发生时：
   - `{6,97,100,115,123,132,188}` -> flags 3
   - `{13,45,101,189}` -> flags 1
   - 其他 -> 0

该算法对第一批3对+第二批13对，共 **16/16** 成功 target 的全部 226 records 逐字节匹配。

历史 Coffee LKBC converter 同样使用 226 项及上述特殊 flags；最终裁决仍以成功 target 为主。

## 4. External .anim

FelReaver `FelReaver0069-00.anim`：335 / 成功112 SHA256 均为 `ceeea2f2db07411ebc9a6db711435332310a7305e11ba82ef4fe32b02c0493d0`。

Muru `Muru0060-00.anim`：两边 SHA256 均为 `bee3b37fe6e9b36499f8c23da457b342530d6c83e6b4a36720c2ecfde9af33e4`。

当前证据支持 external `.anim` payload 原样复制。

## 5. Multi-view / skin

AO_Windmill、AGS_Engines 均为 2 Views。source skin00/01 与 target embedded View0/1：indices、triangles、properties、TextureUnits 逐字节一致，LOD 保留；Classic submesh 等于 WotLK submesh 前32字节。

因此 converter 不应限制 `nViews==1`，应遍历全部 source skin profile 并嵌入。

## 6. TextureAnimation

VR_Elevator_Lift translation track source per-animation counts 为 `[3,69,33,70,0]`；成功 target ranges 为 `(0,2),(3,71),(72,104),(105,174),(175,176),(0,0)`，times=177、keys=177，完全符合现有 generic Legacy Range/Times/Keys 算法。

TextureTransform/TextureAnimation 复用统一 legacy track serializer。

## 7. Ribbon 尚未验证

名为 `Auchindoun_Ethereal_Ribbon_Type3.m2` 的样本真实 header `nRibbonEmitters=0`，它实际上是 TextureAnimation 模型。

下一批必须按 header 自动筛 `nRibbonEmitters>0`，不能按名字猜。

## 8. Particle

第二批共覆盖 50 个 emitter：source stride 476B，target stride 504B。50/50 flags 只观察到清除 WotLK 高位：37个 clear `0x20000`，12个 clear `0x40000`，1个 clear `0x60000`；未观察到 target 新增 bit。

但内部固定字段/track 映射仍非 raw-copy，FULL Particle writer 暂不解锁。

## 9. BLP

第二批可成对比较的普通 3D BLP 37 对，37/37 SHA256 完全一致。继续采用“普通兼容 3D BLP 原样保留；UI Icon/cursor 单独处理”。

## 10. 下一步

第三批改成全库 Audit：扫描全部 12,512 成功 v256 M2；对同路径 335 pair 全库验证 Sequence/Index；验证 AnimationLookup 与 V4 226 Playable；按真实 header 自动选择 ribbon/particle/light/camera/texanim/multiview/duplicate AnimID/non-identity Index/external anim 样本并打包。

全库 Audit 通过后才把 Active Bone Animation 提升为 BATCH_READY。
