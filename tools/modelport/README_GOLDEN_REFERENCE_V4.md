# ModelPort Golden Reference V4

V4 是当前 M2 动画元数据权威实现入口。

## 第二批 Golden 验证规模

- 13 组 `335 v264 -> 成功 1.12 v256` 深度对照
- 17 个 embedded View
- 50 个 Particle emitter
- 10 个 TextureAnimation records
- 39 对普通 3D BLP：39/39 SHA256 exact
- 2 对 external `.anim`：2/2 SHA256 exact

## V4 动画规则

### Sequence

```text
Sequence.Index = preserve WotLK source value
```

禁止：

```text
Index = 0
Index = physical sequence position
```

成功 DeathCoil / Sunwell / FelReaver 已证明 `Index != position` 是正常的 alias 语义。

### AnimationLookup

```text
count = max(AnimationID)+1
missing = 0xFFFF
```

重复 AnimationID：优先 `SubAnimationID==0`，如果没有 sub0 才取第一 physical occurrence。

### PlayableAnimationLookup

固定 `226 x 4B`。

**不要再直接用 build12340 AnimationData.dbc field5 生成。**

当前 V4 使用历史 226-era fallback graph，并加入成功 1.12 Golden target 直接观察到的扩展：

```text
170->16  171->16  172->16  173->16  174->16
175->30  176->16  178->16  179->16  181->16
191->159
```

该 V4 graph 对第二批 13/13 成功 target 的全部226项逐项 exact match。

fallback flags：

```text
6/97/100/115/123/132/188 -> 3
13/45/101/189            -> 1
others                    -> 0
```

### 其他已确认

- quaternion compressed short `-1` -> exact float `+1.0`
- sequence timeline：每条 sequence 前 +3333ms
- generic Legacy Range/Times/Keys 已由 bone + TexAnim Golden 样本验证
- external `.anim` 在 FelReaver/Muru 中 byte-identical，可原样复制
- multi-view skin -> embedded Views 已验证 17 views
- 普通兼容 3D BLP 原样保留；UI icon/cursor 单独安全处理

## Particle

第二批已覆盖 50 emitter：

```text
source stride = 476
target stride = 504
emitter count preserved
target flags = source flags & 0xFFFF
```

Fake color/alpha/size 降级已有较强证据，但 Wand 单 Sequence particle track 出现特殊扩展分支，因此 FULL Particle writer 暂不标 BATCH_READY。

## Ribbon

第二批文件名带 Ribbon 的样本真实 header `nRibbonEmitters=0`。Ribbon 尚未 Golden 验证。

第三批必须按 header `nRibbonEmitters>0` 全库自动筛选，不能按文件名猜。

## 当前模块

```text
playable_lookup_v4.py
animation_metadata_v4.py
validate_animation_metadata_v4.py
test_animation_metadata_v4.py
scripts/Pack_ModelPort_GoldenReference_ThirdBatch_AutoScan.ps1
```

详细证据：

```text
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0623.md
```
