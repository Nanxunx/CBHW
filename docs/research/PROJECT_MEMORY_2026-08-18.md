# 模型移植项目长期记忆 — 2026-08-18

本文件用于保存当前“模型移植 / Turtle335Converter”研究结论，供后续分析直接继承。

## 项目目标

将 WoW 3.3.5a build 12340 客户端资源可靠 retroport 到 Vanilla 1.12.x / Turtle WoW 1.18.1 build 7272 可加载格式，并能被 Penqle/tortoise-wow 的 maps/vmaps/mmaps 工具链正确消费。

## 核心方法

禁止“只改版本号 / raw memcpy / 删除未知块后沿用旧 offset”。统一采用：

```text
3.3.5 Reader
 -> Normalized semantic model
 -> semantic downgrade
 -> Vanilla/Turtle Writer
 -> validator
 -> Turtle server extractor regression
```

证据等级统一为：客户端二进制确认 / 源码确认 / 强推断 / 未验证。

## 已确认的服务端地图策略

- TrinityCore 3.3.5 map extractor 有活动的 MH2O 读取逻辑。
- Penqle/tortoise-wow ADT parser 有 MH2O 结构，但实际 map extraction 中 MH2O 处理被注释，主要使用 MCLQ。
- 因此服务端 `.map` 推荐：Trinity-derived 335 reader semantics + Tortoise target `.map` writer。
- 不直接使用 Trinity `.map` 输出，因为 Trinity 与 Tortoise map format/version 不同。
- client ADT 仍需要真正完成 MH2O -> legacy MCLQ retroport。

## M2 关键结论

- 3.3.5 与 Vanilla/Turtle M2 Header/子结构存在真实布局差异，不能只改版本字段。
- server VMAP 对 M2 主要需要 bounding geometry；client retroport 则需要完整 M2 转换。
- Wall-core/M2Workshop 是高价值参考：近期仍维护 Vanilla rotation/animation 修复，适合作为 M2RepairValidator / regression reference，而不是 authoritative 335->Vanilla writer。
- M2Workshop 为 GPL-3.0，优先独立重写算法而非直接复制大量代码。

## WMO 关键结论

- WMO MVER=17 不能用于判断 Vanilla/WotLK。
- 3.3.5 与旧 Vanilla/Tortoise MOPY flags 语义不同，必须 semantic remap，禁止 bit shift/raw copy。
- 3.3.5 MOGP 高位扩展包括第二套 MOCV/MOTV；目标旧客户端需要 downgrade/bake。
- WMO collision/filter/liquid 必须使用目标语义重建。

## Wall-core 调研结论

完整分析见 `docs/research/WALL_CORE_REPO_ANALYSIS.md`。

评级：
- M2Workshop: A — M2 repair/Vanilla compatibility/regression
- VMaNGOS-Default: A- — target extractor/vmap + legacy asset path resolver
- Everlook: B+ — legacy DBC schema + extractor/vmap cross-check
- AshenWoW: B- — fork-specific extractor diff reference
- Everlook-Bugtracker: A-（验证）— terrain/LOS/texture/display runtime regression corpus
- Wallcraft-bugtracker: D — 当前资产转换价值低

## 应新增的横向模块

1. `LegacyAssetPathResolver`
   - exact path
   - slash/case normalization
   - .mdx/.mdl -> .m2
   - underscore <-> space fallback
   - explicit diagnostics: EXACT/NORMALIZED/LEGACY_FALLBACK/MISSING/AMBIGUOUS

2. `M2RepairValidator`
   - header/offset/count validation
   - Vanilla bone rotation representation
   - animation ranges/times/keys
   - embedded view/skin constraints
   - particle/ribbon bounds
   - texture/.skin/.anim dependency checks

3. `LegacyDbcSchemaCrossCheck`
   - compare Mapache WDBXEditor definitions + WoWDBDefs + Everlook dbc.desc/server usage
   - first tables: Map, AreaTable, LiquidType, CreatureModelData, CreatureDisplayInfo, GameObjectDisplayInfo, ItemDisplayInfo

4. `RuntimeRegressionCatalog`
   - terrain seam/height tests
   - WMO/VMAP LOS ray fixtures
   - texture dependency manifest
   - DBC/server display-binding tests

## 当前下一步优先级

P0: 完成 ADT Writer（MHDR/MCIN/256xMCNK + offset rebuild）并做 client/server双侧验证。
P0: 完成 M2 v264-source -> Vanilla/Turtle target 的字段级结构表及 writer。
P1: 实现 LegacyAssetPathResolver。
P1: 建立 M2RepairValidator。
P1: 做 WMO MOPY/material/UV2/color2 semantic downgrade。
P2: 建立 DBC schema cross-check 和 runtime regression corpus。

后续研究应优先寻找能补充这些空白的开源项目，而不是重复研究普通 MaNGOS gameplay core。
