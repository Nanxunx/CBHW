# ModelPort Golden Reference — V3 已由 V4 取代

目标仍然是标准 `MD20 v256`；没有 `.orange`、没有私有 Header、没有 Orange DLL 运行依赖。

## 重要纠错

第二批 13 个 Golden pairs 证明 V3 的一条假设错误：

```text
错误：Sequence.Index = physical sequence index
正确：Sequence.Index = 原样保留 WotLK source Index
```

`AnimationLookup` 才使用 physical sequence index：

```text
count = max(AnimationID)+1
missing = 0xFFFF
lookup[AnimationID] = first physical sequence index
```

因此 V3 repair/validator 已改成 fail-closed，禁止继续生成或认可猜测 Index 的模型。

## 当前版本

请使用：

```text
playable_lookup_v4.py
animation_metadata_v4.py
validate_animation_metadata_v4.py
test_animation_metadata_v4.py
```

V4 还加入了从 build12340 `AnimationData.dbc` 生成 226-entry model-aware `PlayableAnimationLookup` 的规则。

详细证据：

```text
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0534.md
```
