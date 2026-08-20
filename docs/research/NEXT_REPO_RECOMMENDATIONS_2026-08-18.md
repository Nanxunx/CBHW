# 下一批推荐源码仓库 — 2026-08-18

目标：继续寻找可用于 WoW 3.3.5a build 12340 -> Vanilla/Turtle 1.18.1 资产 retroport 的源码参考。

## 推荐优先级

### S 级：wowemulation-dev/warcraft-rs

许可证：MIT OR Apache-2.0。

Workspace 已实际拆成独立 crates：
- wow-mpq
- wow-adt
- wow-wdt
- wow-wdl
- wow-blp
- wow-m2
- wow-wmo
- wow-cdbc
- CLI

对本项目价值很高，因为它同时把 Vanilla 与 WotLK 放在一个统一类型系统中，且存在 parser / validator / writer / converter 结构。

但重要警告：不要直接信 README 的“Full Support / Version Conversion”。源码审计发现：

1. ADT `wotlk_to_tbc()` 调用了 `convert_mh2o_to_mclq()`，但后续只把 MCNK `liquid_offset/liquid_size` 临时写成 1，并明确标记为 Placeholder；反向 TBC->WotLK 甚至创建默认/空 MCLQ 占位数据。
2. M2 `M2Model::convert()` 当前显式转换 header/vertices/textures/bones/materials，然后 clone 其余数据；animation/particle/ribbon/embedded skin 等不能因此视为已经完成可靠跨版本转换。
3. `version.rs` 中存在若干“empirical/theoretical”版本映射，必须与我们的真实客户端二进制和 Trinity/Tortoise 源码交叉验证，不能直接升格成真值。

正确利用方式：
- 重点审计 parser/writer/normalized structs。
- 借鉴 Rust 类型建模、validator、offset rebuild、round-trip tests。
- 将其 converter 当待验证算法候选，而不是 authoritative converter。
- 特别深入：wow-adt liquid_converter、ADT builder/writer、wow-m2 embedded_skin/header/bone/animation、wow-wmo group/material/liquid。

评级：S（研究价值） / B（直接 converter 可信度，当前需审计）。

---

### S- 级：wowdev/noggit3

定位：当前主要 3.3.5a 地图编辑器，GPL-3.0。

非常适合补 ADT 地图侧细节。近期 release 明确提供“导出 old liquid format (MCLQ) for older clients”，这直接命中我们的 MH2O -> MCLQ 路线。

重点研究：
- old-liquid/MCLQ save path
- MCNK offset rebuild
- alpha map/MCAL save
- liquid layers and fatigue/deep-water flags
- holes/impassable/ground effects
- WDT/WDL regeneration
- model/WMO placement save logic

Noggit issue #87 还给出 ground-effect bitmask 实际解释，可用于补 MCNK doodad/ground-effect 字段语义。

正确利用方式：从当前保存器中提炼 ADT semantic writer 规则，和我们的 Turtle target loader 对照。不要整套 vendoring。

评级：S-（ADT）。

---

### A+：WowDevs/jM2converter + jM2lib lineage

仓库本体很小，CLI 只是：
- BlizzardInputStream 读取 M2/MD21
- 调用 `model.convert(version)`
- BlizzardOutputStream 写回

真正的转换核心在外部/依赖的 jM2lib 中。因此单独看 jM2converter 不够，下一步应追踪 jM2lib 的具体仓库、版本和历史实现。

价值：这是 M2Workshop README/workflow 已经引用过的老牌 M2 跨版本路线，且 README 明确说明 retroport 会损失新版本功能，并列出 ribbon、color、shader 等已知问题。这些问题与我们当前 M2 downgrade 风险高度一致。

推荐：找到 jM2lib 后逐类对比 Bone/Animation/Skin/Particle/Ribbon 的 version converter。

评级：A+（M2历史算法参考）。

---

### A：wowdev/pywowlib

MIT。

实际 README 对支持范围比较诚实：
- M2：3.3.5a 完整标记，Vanilla 仅部分
- WMO：1.12.1/2.4.3/3.3.5a 均支持，但注明 WotLK+ 仍有变化待补
- ADT：3.3.5a 虽标记支持，但 README 明确说从未测试、可能损坏并计划重写
- MPQ：Classic/WotLK 支持
- BLP：读取/PNG
- DBC：Classic/WotLK

因此它特别适合作为 Source335 parser/WMO cross-check，而不是 Vanilla writer authority。

评级：A（独立 parser/cross-check）。

---

### A：WowDevTools/Blender-WMO-import-export-scripts

GPL-3.0。

WMO 编辑能力覆盖：
- geometry
- UV
- materials/shaders/blending/flags
- vertex colors
- portals
- fog
- liquids
- collision
- render batches
- lightmaps

这是 WMO semantic reconstruction 的高价值来源。尤其适合帮助我们理解：
- MOMT shader/material
- MOPY collision
- portal/batch rebuild
- MLIQ
- vertex color / lightmap

风险：项目 README 明确警告 WIP/可能产生文件损坏，因此不能把 writer 直接当 authoritative target writer。

评级：A（WMO语义参考）。

---

### A-：Kruithne/wow.export

当前活跃，支持 legacy MPQ 本地浏览、M2/WMO preview/export、legacy MPQ map preview、terrain export，并具备 BLP/模型/地图依赖解析能力。

不适合当 retroport writer，但非常适合作为：
- Source335 dependency scanner
- M2/WMO/ADT geometry cross-check
- texture/material metadata validator
- 输出 OBJ/GLTF 后做几何差异对比

评级：A-（验证工具）。

---

### A-：Zaelemgaad/WoW-Crucible

这是一个很新的 3.3.5a 内容编辑/patch MPQ 工具，README 声明主验证目标 12340，并提供 Classic 5875 target profile。项目仍明确处于 early development。

值得进一步研究的方向：
- ADT/WDT object placement transactions
- MMID/MMDX/MDDF 与 MWID/MWMO/MODF 重建
- UID 分配与跨 ADT duplication
- MCRF rebuild
- WMO bounds/placement coordinate mapping
- MPQ patch manifest / atomic output
- target-profile abstraction

它可能不是旧格式 converter，但其“结构化地图编辑 + 精确重建引用”的设计思想与 Turtle335Converter 很接近。

评级：A-（架构/ADT object placement）。

---

## 下一步建议研究顺序

1. `warcraft-rs` 深挖：ADT `liquid_converter`、builder/writer；M2 `embedded_skin`、bone/animation/particle/ribbon writer。
2. `noggit3` 找出 old MCLQ 导出实现，逐字段与我们的 `ADT_MH2O_MCLQ_CONVERSION.md` 对照。
3. 追踪 `jM2lib` 真正源码，建立 M2 3.3.5 -> Vanilla converter 差异表。
4. `Blender-WMO-import-export-scripts` 对比 MOMT/MOPY/MOBA/portal/MLIQ writer。
5. `WoW-Crucible` 对比 MDDF/MODF/UID/MCRF rebuild。
6. `pywowlib` 和 `wow.export` 作为第三方 parser/visual validator。

## 结论

当前最值得投入的两个新来源：

- `warcraft-rs`：因为它提供统一的双版本数据模型、writer、validator，适合大量借鉴工程结构；但 converter 实现尚有 Placeholder，必须审计。
- `noggit3`：因为最新代码已经真正涉及“old liquid format MCLQ for older clients”，这与当前 ADT retroport 的核心缺口完全重合。
