# ModelPort Batch Harness V3 — Golden Reference

目标仍然是标准 `MD20 v256`，没有 `.orange`、没有私有 Header、没有 Orange DLL 运行依赖。

## V3 新增

`animation_metadata_v3.py`

已经实现 Golden Reference 验证后的动画元数据规则：

- Sequence Index 按真实 sequence index 写入
- 自动生成 Classic AnimationLookup
  - count = max(AnimationID)+1
  - 缺失 = 0xFFFF
  - lookup[AnimationID] = SequenceIndex
- PlayableAnimationLookup 保持 226 项，但根据模型实际 AnimationID 更新
- quaternion compressed `-1` 精确转 `+1.0f`
- 保留已验证的 Legacy Range / Times / Keys
- 不强制 16-byte alignment

`validate_animation_metadata_v3.py`

可在进客户端前拦截：

- Sequence Index 全零
- AnimationLookup 缺失/过短
- Playable 226 项异常
- 当前模型实际动画 ID 没有写进 Playable

## 当前状态

Active Bone Animation 已从“盲猜格式”推进为：

`REFERENCE_ALIGNED_IMPLEMENTATION_READY_FOR_GOLDEN_TEST`

仍建议先用第二批 Golden Reference 验证规则泛化，再进行一次真实客户端 Animated Golden test。
