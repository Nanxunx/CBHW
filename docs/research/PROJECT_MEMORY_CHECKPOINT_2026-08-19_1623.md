# 模型移植项目检查点 — 2026-08-19 16:23 +08:00

权威仓库：`NansenCore/Turtle335Converter` / `main`。

## 当前最高优先级：M2 3.3.5a -> Turtle 1.18.1

V4.4 定向扫描有效：18083 source M2、11596 paired、5036 high-risk deep、deep_errors=0。

稳定生产规则：

- `AnimationLookup`: 5036/5036 PASS；count=maxID+1，missing FFFF，重复ID优先 SubID0。
- Sequence.Index：保留 source，不能写0也不能等同 physical position。
- ordinary legacy Range/Times/Keys、embedded View、quaternion -1->+1.0 等旧结论继续有效。

V4.5 新结论：

- Playable 主 fallback 图来自 build12340 `AnimationData.dbc` raw field index 5，必须 model-aware 递归。
- Golden overrides：121->14, 146->0, 172->16, 174->16, 181->19, 191->159。
- 29 个已上传 selected Golden / 6554 Playable records exact PASS；仍须本地重验旧 498 failures，暂称 provisional。
- Ribbon semantic writer：13 models / 448 emitters exact PASS，状态 GOLDEN_READY_OFFLINE_SEMANTIC。
- Particle semantic writer：24 models / 930 emitters exact PASS；nonzero nUnknownReference/ofs 是 BLOCKER；WotLK-only 丢失字段必须显式报 LOSS。

V4.4 待补：29 Sequence mismatch / 27 timeline mismatch，文件由 V4.5 refinement 脚本自动打包，不再全库重扫。

## C++ 工具化进展

项目之前只有 ADT/DBC C++ 实现，没有 `src/m2`。本检查点开始真正 M2 C++ core：

- WotlkM2Reader
- AnimationMetadata
- turtle335_probe_m2
- test_m2_core

目标架构仍是：Reader -> normalized semantic model -> downgrade -> target v256 writer -> validator -> real client regression。

禁止 Orange/private output；历史 LKBC/Coffee 仅作算法参考。
