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
- **客户端优化后的 multi-record MCLQ 不应再被 Tortoise old-MCLQ map extractor 反抽为服务器 `.map`；服务器继续直接读原始 335 MH2O。**

## 2026-08-18 MCLQ multi-record 重大修正

完整证据见 `docs/research/NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md`。

Turtle 1.18.1 真客户端 `0x6AF760` 已确认 legacy MCLQ 按 MCNK category bits 固定遍历四类：

```text
0x04 River/Water
0x08 Ocean
0x10 Magma
0x20 Slime
```

每个存在的类别消费一份独立 804-byte record，顺序固定为：

```text
River -> Ocean -> Magma -> Slime
```

Noggit3 old-MCLQ writer 与这一顺序完全吻合，并使用：

```text
MCNK.sizeMCLQ = 8 + 804 * recordCount
MCLQ inner chunk size = 0
```

因此旧项目设计中“把不同液体类别强行合并到单一 9×9 MCLQ grid”已被新证据取代。正确设计是：

```text
MH2O instances
 -> group by target category
 -> merge same-category instances
 -> at most one record per category
 -> write records in River/Ocean/Magma/Slime order
```

重要 cell code 修正：

```text
Ocean = 1
Slime = 3
River/Water = 4
Magma/Lava = 6
Hidden = 0x0F
```

Noggit `mclq_tile` 还明确建模：

```text
bit6 = fishable
bit7 = fatigue
```

所以此前“fishable 无 legacy 编码”的结论不再成立；最终 bit6/bit7 policy 继续以 Turtle fixture + Noggit 交叉验证。

已完成代码重构：`LegacyLiquid / MclqWriter / McnkWriter / tests` 已改为 category-grouped multi-record 模型，并覆盖 Water+Ocean=1616B、四类=3224B、Slime=0x03、same-category conflict、cross-category overlap 等测试。

## M2 关键结论

- 3.3.5 与 Vanilla/Turtle M2 Header/子结构存在真实布局差异，不能只改版本字段。
- server VMAP 对 M2 主要需要 bounding geometry；client retroport 则需要完整 M2 转换。
- Wall-core/M2Workshop 是高价值参考：近期仍维护 Vanilla rotation/animation 修复，适合作为 M2RepairValidator / regression reference，而不是 authoritative 335->Vanilla writer。
- M2Workshop 为 GPL-3.0，优先独立重写算法而非直接复制大量代码。
- WoW-Crucible `StaticM2DownportService` 是 modern MD21/v274 -> WotLK v264，不是 335->Vanilla；其 loss/blocker/accounting 工程模式值得借鉴。

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

## 新仓库调研结论

- `wowdev/noggit3`：ADT old-MCLQ writer 已提升为 target-side 高价值参考；其 MCLQ multi-record 行为与 Turtle 真客户端一致。
- `wowemulation-dev/warcraft-rs`：parser/struct/test 架构价值高，但 ADT liquid converter 只取 `instances[0]` 且外层 converter 有 Placeholder，不能直接承担本项目液体转换。
- `jM2converter`：CLI 很薄，真正算法在 jM2lib lineage，后续应追真正的 jM2lib/M2Lib 实现。
- `Zaelemgaad/WoW-Crucible`：ADT placement pipeline 价值很高；完整审计见 `WOW_CRUCIBLE_PLACEMENT_AUDIT_2026-08-18.md`。本体 MIT。

## WoW-Crucible placement 结论

Crucible 当前 WotLK ADT placement 实现已经确认：

```text
MMDX/MMID -> MDDF (36B)
MWMO/MWID -> MODF (64B)
per-MCNK MCRF rebuild
MHDR backpatch
MCIN 256-entry backpatch
map-wide M2/WMO shared UID occupancy
bounds-based cell/tile references
multi-tile atomic publication
```

特别值得移植到 Turtle335Converter 的 invariants：

1. NameId 是 MMID/MWID index position，不是字符串 byte offset。
2. 删除 MDDF/MODF record 后，所有大于被删 index 的 MCRF references 必须减 1。
3. MCRF 大小改变后，要 shift 所有后续 MCNK subchunk offsets 并更新 MCNK size。
4. full writer 应重新计算 top-level positions，然后统一 backpatch MHDR + MCIN。
5. UniqueID 应视为 map-wide namespace；M2/WMO 共用 occupancy validator。
6. WMO MODF bounds 在 rotate/scale 时必须从 exact WMO root MOHD bounds 重算；仅 translation 可对已有 world extents 加 delta。
7. 大型 object 跨 ADT tile 时所有 touched tiles 应作为一个协调事务处理，不能只写主 tile。

因此完整 ADT Writer 不应复制源 placement chunks，而应从 NormalizedADT semantic placement tables 重新生成：

```text
canonical path catalogs
 -> MMDX/MMID + MWMO/MWID
 -> MDDF/MODF
 -> per-cell MCRF
 -> 256 MCNK
 -> MCIN
 -> MHDR
 -> reopen/validate
```

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

5. `MapPlacementUidRegistry`
   - preserve valid source UID
   - detect M2/WMO/map-wide collision
   - deterministic remap only when repair is required

6. `ConversionManifest`
   - input hashes
   - transformations
   - losses/blockers
   - output hashes
   - validation results

## 当前下一步优先级

P0: **完成 full Vanilla/Turtle ADT root writer。**

顺序：

```text
1. canonical M2/WMO path catalogs
2. MMDX/MMID + MWMO/MWID serialization
3. MDDF/MODF serialization preserving source UID/transform
4. per-cell MCRF rebuild
5. serialize 256 MCNK
6. MCIN writer
7. top-level chunks
8. MHDR backpatch
9. reopen + strict validate
10. Noggit + Turtle client fixture regression
```

P0: server `.map` 仍从 original 335 MH2O 走 Trinity-style reader -> Tortoise z1.4 writer，不能从 client ADT multi-record MCLQ 反抽。

P0: full ADT fixture成功后，进入 M2 v264 -> Vanilla/Turtle v256 writer，优先审计 actual jM2lib/M2Lib + M2Workshop + Turtle loader。

P1: 实现 `MapPlacementUidRegistry`、`LegacyAssetPathResolver`、`ConversionManifest`。
P1: WMO MOPY/material/UV2/color2 semantic downgrade。
P2: DBC schema cross-check + runtime regression corpus。

后续研究应优先寻找能补充这些空白的开源项目，而不是重复研究普通 MaNGOS gameplay core。
