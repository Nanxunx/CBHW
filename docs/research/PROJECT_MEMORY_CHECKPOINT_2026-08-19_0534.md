# 模型移植项目 — Golden Reference V4 最新记忆检查点

更新时间：2026-08-19 05:34 +08:00

权威源码仓库：`NansenCore/Turtle335Converter`，分支 `main`。

## 数据基线

- 3.3.5a Build12340 原始资源：`<BUILD12340_SOURCE_ROOT>`
- 已成功降到 1.12 的参考库：`<CLASSIC_GOLDEN_ROOT>`
- 成功参考 M2：12,512
- 同路径 335↔112 M2 pairs：11,597
- 第一批 3 对 + 第二批 13 对 = 16 个已深入二进制对照 Golden pairs

## V4 对 V3 的关键纠错

V3 曾错误推断 `Sequence.Index = physical sequence index`。第二批已否定。

正确：

```text
Sequence.Index = 原样保留 WotLK source Index
AnimationLookup[AnimationID] = 第一个 physical sequence index
```

FelReaver 明确存在 physical3/4 对应 source Index4/3，以及 physical9/10 对应 Index10/9，成功 target 原样保留。

因此 repair 如果旧 target 已丢失 Index，必须重新读取 source v264，不能猜。

## Sequence / AnimationLookup

第二批 13/13 的全部 Sequence 字段语义一致，仅 `length -> start/end`，timeline 为每个 sequence 前增加 3333ms。

AnimationLookup 13/13：
- count=max(AnimationID)+1
- missing=0xFFFF
- duplicate AnimationID 取第一个 physical sequence
- SubAnimation 不改变 first-physical rule

## PlayableAnimationLookup V4

固定 226×4B。

生成：build12340 `AnimationData.dbc` field5 Fallback 递归到模型实际存在 AnimID，再加 Golden overrides：

```text
172->16
174->16
181->16
191->159
```

fallback 发生时 flags：

```text
{6,97,100,115,123,132,188} -> 3
{13,45,101,189} -> 1
其他 -> 0
```

该算法已逐字节匹配 16/16 Golden pairs 的全部 226 records。

## External .anim

FelReaver0069-00.anim 与 Muru0060-00.anim 在 335/成功112 中大小和 SHA256 完全相同。当前策略：external `.anim` payload 原样复制。

## Multi-view

AO_Windmill 与 AGS_Engines 2-view 样本已验证：skin00/01 -> embedded View0/1，indices/triangles/properties/TextureUnits byte-identical，LOD 保留，Classic submesh=source submesh 前32B。取消 `nViews==1` 架构限制，循环全部 profiles。

## TextureAnimation

VR_Elevator_Lift 多 sequence texanim 验证现有 generic Legacy Range/Times/Keys 算法正确，TextureTransform 复用统一 serializer。

## Ribbon

第二批所谓 Ribbon 样本真实 `nRibbonEmitters=0`，所以 Ribbon 仍未验证。下一批必须按 header `ribbons>0` 自动筛样本。

## Particle

第二批共 50 emitter：v264 stride476 -> v256 stride504。flags 仅观察到清高位：37个 `0x20000`、12个 `0x40000`、1个 `0x60000`；未见 target 新增 bit。内部映射仍需真实样本群继续反推，FULL Particle 未解锁。

## BLP

第二批普通 3D BLP 可直接成对比对 37 对，37/37 SHA256 相同。3D兼容纹理原样保留；UI icon/cursor 单独安全转换。

## 源码状态

V4 新模块：
- `tools/modelport/playable_lookup_v4.py`
- `tools/modelport/animation_metadata_v4.py`
- `tools/modelport/validate_animation_metadata_v4.py`
- `tools/modelport/test_animation_metadata_v4.py`

V3 的 `Index=physical` repair 必须废弃/fail-closed。

## 下一步

运行第三批全库 Audit：
- 扫 12,512 成功 target M2
- 全库验证 AnimationLookup
- 全库验证 226 Playable V4
- 对同路径 source pair 验证 Sequence metadata / source Index
- 按真实 header 自动选 ribbon/particle/light/camera/texanim/multiview/duplicate AnimID/SubAnimation/nonidentity Index/external anim
- 自动打包代表样本

全库 Audit 通过后，才把 Active Bone Animation 标成 BATCH_READY。
