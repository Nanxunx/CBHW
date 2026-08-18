# ModelPort Golden Reference V4

V4 是当前 M2 动画元数据权威实现入口。

## 已确认

- target = standard MD20 v256
- Sequence 64B -> 68B
- `Sequence.Index` 原样保留 source 值
- `AnimationLookup[AnimationID]` 指向第一个 physical sequence
- PlayableAnimationLookup = 226×4B，按 build12340 AnimationData fallback + Golden overrides 动态生成
- quaternion compressed short `-1` -> exact float `+1.0`
- generic Legacy Range/Times/Keys 已由 bone + TexAnim Golden 样本验证
- external `.anim` 在 FelReaver/Muru 样本中 byte-identical，可原样复制
- multi-view skin00/01 -> embedded View0/1 已验证

## 当前模块

```text
playable_lookup_v4.py
animation_metadata_v4.py
validate_animation_metadata_v4.py
test_animation_metadata_v4.py
```

## 尚未解锁

- FULL Particle writer：476B -> 504B 内部映射仍需继续反推
- Ribbon writer：第二批所谓 Ribbon 样本真实 header ribbons=0，必须从全库按 header 重新取样

详细证据：

```text
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0534.md
```
