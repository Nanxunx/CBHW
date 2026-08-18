# 模型移植项目 — 最新记忆入口

更新时间：2026-08-19 05:34 +08:00

当前优先读取顺序：

1. `PROJECT_MEMORY_CHECKPOINT_2026-08-19_0534.md` — 最新 M2 Golden Reference V4 / 第二批结论。
2. `M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md` — 13 对第二批成功降版模型的详细二进制证据。
3. `PROJECT_MEMORY_CURRENT.md` — ADT/WMO/长期稳定结论。
4. `PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md` — ADT 批量 Probe 与 16/16 CI 历史检查点。

如果旧 M2 结论与 2026-08-19 05:34 检查点冲突，以 05:34 检查点、成功 1.12 Golden 文件和真实客户端为准。

特别注意：V3 的 `Sequence.Index == physical sequence index` 假设已经废弃；正确规则是保留 source Index，而 AnimationLookup 单独指向 first physical sequence。
