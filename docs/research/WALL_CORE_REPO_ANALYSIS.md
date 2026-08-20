# Wall-core repository analysis for Turtle335Converter

状态：2026-08-17 研究快照。

目标：分析 Wall-core 相关公开仓库，判断其对 WoW 3.3.5a build 12340 -> Turtle WoW 1.18.1 / Vanilla-derived 资产移植项目的直接复用价值、算法参考价值和回归测试价值。

证据等级：

1. `SOURCE-CONFIRMED`：已直接检查仓库源码/提交。
2. `ISSUE-CONFIRMED`：已直接检查公开 issue。
3. `INFERENCE`：从源码结构和项目用途推导，仍需样本验证。

---

## 1. Wall-core/M2Workshop

仓库：`Wall-core/M2Workshop`

许可证：GPL-3.0。

结构非常小，核心几乎全部集中在：

```text
README.md
Wallcraft M2 Workshop.1sc
LICENSE
```

它不是一个现代 C++/Rust library，而是 C# OneScript/010-Editor 风格的单体 M2 Workshop 脚本，README 说明其源自 zEzha/M2Mod 工作流，并依赖 M2Redux、Java/jM2Converter、M2Mod3、CASC/texture 工具等外部程序。

### 直接确认的 M2 能力

`SOURCE-CONFIRMED`

脚本内部存在完整的 M2 编辑数据模型和大量操作，包括：

- M2 header/array handling
- animations / sequences
- bones and rotation tracks
- textures / render flags / transparencies
- attachments / events / cameras
- particles
- skin profile files
- external `.anim` files
- model insertion / scaling / vertex operations
- file dependency management

WotLK workflow 会根据 M2 view/skin profile 数量寻找类似：

```text
Model00.skin
Model01.skin
...
```

同时存在 WotLK external animation 文件处理逻辑。

### 对 Vanilla 的真实维护价值

`SOURCE-CONFIRMED`

该项目不是“旧工具完全停更”。仓库提交记录显示 2026-05-02 仍有更新；2025-05-06 的提交明确为：

```text
Update bone rotation tracks to C4Vector for Vanilla
```

该修改针对旧版本 bone rotation track 的 quaternion/rotation value 处理增加版本分支，说明维护者实际遇到并修过 Vanilla 模型旋转兼容问题。

这类修复对 Turtle335Converter 很有价值，因为我们已经由真实 3.3.5/Turtle 客户端证明：

```text
WotLK Bone = 88 bytes
Vanilla Bone = 108 bytes
```

且 rotation track 在跨版本时必须真正重建，不能 raw-copy。

### 不应直接当成 335 -> Vanilla authoritative converter

`SOURCE-CONFIRMED`

M2Workshop 的工作流仍调用外部 jM2Converter 等工具；它更像：

```text
model editor
+ repair workstation
+ batch model workflow
```

而不是一个自包含的 build12340 -> v256 writer。

因此不要把整个 `.1sc` 直接搬进 Turtle335Converter。

### 推荐借鉴

高价值：

1. Vanilla Bone/rotation repair heuristics。
2. animation range/track repair 思路。
3. particle/ribbon 编辑和 sanity-check 逻辑。
4. 外部 `.skin/.anim` 文件发现与依赖管理 UX。
5. 模型 batch repair/rename/texture dependency workflow。
6. 用它作为生成后人工/自动回归工具，检查我们的 v256 输出是否还需要 repair。

建议项目模块：

```text
M2RepairValidator
M2DependencyScanner
M2WorkshopRegressionFixtures
```

许可证注意：GPL-3.0。若直接复制其实现代码，会带来 GPLv3 许可证约束。当前更推荐“理解算法 + 独立实现 + 用其输出作交叉验证”。

总体评级：**A / 高价值参考，低到中直接代码复用价值。**

---

## 2. Wall-core/VMaNGOS-Default

仓库：`Wall-core/VMaNGOS-Default`

默认分支：`development`。

许可证：GPL-2.0。

这是完整 Vanilla/VMaNGOS 服务端 fork，主要结构：

```text
src/
sql/
contrib/
dep/
LuaScripts/
Bin/
```

对 Turtle335Converter 最相关的是：

```text
contrib/extractor/
contrib/vmap_extractor/
contrib/mmap/
contrib/model_reader/
```

### Vanilla M2 target-side cross-check

`SOURCE-CONFIRMED`

`contrib/vmap_extractor/vmapextract/modelheaders.h` 使用旧 Vanilla 风格 M2 header，包括：

```text
nD / ofsD
nViews / ofsViews
nI / ofsI
```

该文件与 Wall-core/Everlook 中的同名文件 blob SHA 完全一致。

意义：Wall-core 的实际 Vanilla VMAP 工具链长期使用的 M2 collision reader 与我们此前从 Turtle 客户端和 Penqle/tortoise-wow 得到的目标结构一致。

用途：作为 **独立 target-side reference**，不需要复制。

### GameObject 模型路径解析器值得借鉴

`SOURCE-CONFIRMED`

`contrib/vmap_extractor/vmapextract/gameobject_extract.cpp` 有一个非常实用的 legacy MPQ dependency resolver：

- 从 `GameObjectDisplayInfo.dbc` 取 displayId -> model path。
- `.mdx` / `.mdl` 规范化到 `.m2`。
- 只接受预期 `.m2/.wmo` 类型。
- 当 MPQ 中路径不精确匹配时尝试：
  - 原字符串；
  - `_` 替换成空格；
  - 空格替换成 `_`；
  - legacy 命名兼容 fallback。

这是非常适合直接**独立重写**进 Turtle335Converter 的逻辑。

推荐新模块：

```cpp
LegacyAssetPathResolver
{
    exact path
    case-normalized path
    extension normalization
    underscore_to_space fallback
    space_to_underscore fallback
}
```

为什么有价值：批量处理私服/老 MPQ 时，“DBC 引用存在、文件也存在，但历史文件名空格/下划线不同”会造成大量假 Missing Asset。这个 resolver 可以直接降低失败率。

### extractor/vmap/mmaps 的正确定位

`SOURCE-CONFIRMED`

这些工具适合作为：

```text
Vanilla target semantics
VMAP geometry regression
server-side map acceptance
```

而不是 3.3.5 client-format reader。

不要把 VMaNGOS extractor 当成 WotLK MH2O/M2 v264 转换器。

总体评级：**A- / 很高 target-side 和路径解析参考价值。**

---

## 3. Wall-core/Everlook

仓库：`Wall-core/Everlook`

默认分支：`dev2`。

许可证：GPL-2.0。

它是完整服务端项目，`contrib/` 除常规：

```text
extractor
vmap_extractor
mmap
model_reader
```

外，还包含：

```text
dbcEditer
dbcformat
```

### extractor 是旧 MCLQ/Vanilla target reference

`SOURCE-CONFIRMED`

其 extractor `System.cpp` 直接消费 MCNK `MCLQ`：

```text
MCLQ.flags[y][x] != 0x0F -> visible liquid
bit7 -> dark water
MCNK liquid flags -> water/ocean/magma/slime
```

因此它与我们的目标 MCLQ writer 是很好的独立回归源。

但它不是 3.3.5 MH2O reader，不应该反过来作为 Source335 parser。

### legacy DBC schema reference 值得保留

`SOURCE-CONFIRMED`

`contrib/dbcformat/dbc.desc` 是 CMaNGOS/Vanilla 风格 DBC table format descriptor，`dbcEditer` 又带独立旧式 DBC reader/editor。

对 Turtle335Converter 最合适的用途不是替代 Mapache WDBXEditor2/WoWDBDefs，而是组成第三套 schema cross-check：

```text
Mapache WDBXEditor 5875 XML
       +
WoWDBDefs
       +
Everlook dbc.desc / server expectations
```

当一个 Vanilla DBC 字段在两个现代定义来源有分歧时，可以查 Everlook 服务端实际如何消费该表。

推荐模块/测试：

```text
LegacyDbcSchemaCrossCheck
```

### VMap fork 差异

`SOURCE-CONFIRMED`

Everlook 的 `modelheaders.h` 与 VMaNGOS-Default 完全相同，但 `gameobject_extract.cpp`、`model.cpp`、`wmo.cpp` 等并非全部相同。

这意味着：

- 核心 old M2 target structure 并未改变；
- 不同项目曾对模型/WMO抽取行为做过项目级调整。

后续若碰到某类 VMAP 失败，值得用：

```text
Penqle/tortoise-wow
VMaNGOS-Default
Everlook
AshenWoW
```

做四方 diff，而不是在 converter 中盲目增加 workaround。

总体评级：**B+ / DBC、target extractor、历史差异参考价值高。**

---

## 4. Wall-core/AshenWoW

仓库：`Wall-core/AshenWoW`

默认分支：`development`。

许可证：GPL-2.0。

结构与 VMaNGOS fork 高度相似：

```text
src/
sql/
contrib/
dep/
LuaScripts/
```

其 `contrib/` 同样带：

```text
extractor
vmap_extractor
mmap
model_reader
```

### 与 VMaNGOS 重复度很高

`SOURCE-CONFIRMED`

AshenWoW 的 `gameobject_extract.cpp` 与当前 Wall-core/VMaNGOS-Default 版本 blob SHA 完全相同，因此不能把它算成第二个独立算法来源。

### extractor 存在项目级差异

`SOURCE-CONFIRMED`

AshenWoW `System.cpp` 与 VMaNGOS 当前文件并非同 SHA，并且可见项目级配置差异，例如：

- `EXTRACT_CAMERA`
- camera extraction option
- MPQ list差异
- height conversion default差异

这些说明 fork 并非完全镜像，但当前看到的差异主要属于服务端提取流程/项目配置，而不是新的 3.3.5 -> Vanilla asset converter。

建议：只在出现 extractor-specific bug 时做 fork-diff；不要将整个仓库纳入 converter dependency。

总体评级：**B- / 有差异参考，但大量内容与 VMaNGOS 重复。**

---

## 5. Wall-core/Wallcraft-bugtracker

公开 issue tracker。

`ISSUE-CONFIRMED`

当前公开 issue 数量极少，查到的主要是 gameplay 问题，例如：

- Master Elemental Shaper Krixix dialogue
- Songflower cooldown

没有形成可用于模型/地图转换的大规模故障语料。

总体评级：**D / 当前对 Turtle335Converter 基本无直接价值。**

保留观察即可，不需要抓取进 regression corpus。

---

## 6. Wall-core/Everlook-Bugtracker

这是这批项目里一个容易被低估的资源。

它没有可复用 converter 代码，但拥有大量真实 Vanilla 客户端/服务端运行问题，可用于建立 **regression fixture catalog**。

### Issue #342 — Terrain glitch

`ISSUE-CONFIRMED`

Valley of Trials 出现明显 terrain glitch，最终通过新 patch 修复并由玩家确认。

价值：

```text
ADT转换后“能加载” != 地形视觉正确
```

应加入：

- chunk border seam test
- height continuity
- alpha/shadow seam
- client-patch asset version consistency

### Issue #512 — LoS issues

`ISSUE-CONFIRMED`

Sunken Temple 有明显 Line of Sight 问题。维护者明确指出 maps 是从客户端直接生成的，但生成结果仍不完美。

价值非常高：

```text
client asset -> extractor -> vmap
```

成功跑完，不代表 collision/LOS 正确。

Turtle335Converter 应增加 VMAP regression：

- wall blocking
- door/opening
- stairs
- room interiors
- WMO BSP
- portal transitions
- LOS ray pairs

### Issue #77 — Missing texture

`ISSUE-CONFIRMED`

Marshlands 的资源节点出现黄色/缺失纹理问题；讨论指向 client visual patch，并提及其他区域也有类似 missing texture。

价值：建立 dependency validator：

```text
M2/WMO valid
    !=
all referenced BLPs resolved
```

建议每个转换模型输出 dependency manifest：

```text
model.m2
  -> texture A OK
  -> texture B MISSING
  -> skin OK
  -> anim OK
```

### Issue #21 — incorrect weapon model

`ISSUE-CONFIRMED`

Cho'Rush the Observer 在不同职业形态应该持不同武器，但服务端始终显示 Mage weapon。

这主要是服务器 display/equipment state 问题，不是文件格式问题。

仍值得提醒：转换器 Validator 不能把“模型文件加载成功”与“正确 DisplayInfo/Equipment 映射”混为一谈。

因此 DBC/server binding regression 与 binary asset regression 应分开。

总体评级：**A- / 非常好的真实回归案例库，低直接代码价值。**

---

# 7. 可直接带回 Turtle335Converter 的内容

## 7.1 LegacyAssetPathResolver — 推荐实现

来源思想：Wall-core/VMaNGOS-Default `gameobject_extract.cpp`。

功能：

```text
Resolve(path):
  1 exact
  2 normalized slashes
  3 extension .mdx/.mdl -> .m2 where semantically valid
  4 case-insensitive MPQ lookup
  5 underscore -> space
  6 space -> underscore
  7 report canonical resolved path
```

必须记录诊断：

```text
EXACT
NORMALIZED
LEGACY_NAME_FALLBACK
MISSING
AMBIGUOUS
```

不要静默 fallback。

优先级：**高**。

---

## 7.2 M2RepairValidator — 推荐实现

来源思想：M2Workshop + 当前客户端二进制研究。

首批规则：

```text
Header version 256
embedded View count/offset
Bone rotation representation
legacy Animation ranges/times/keys
skin/submesh bounds
Particle/Ribbon track bounds
texture references
external .skin/.anim must not remain required for canonical v256 output
```

M2Workshop 作为 repair/reference tool，而不是 writer authority。

优先级：**高**。

---

## 7.3 LegacyDbcSchemaCrossCheck — 推荐建立测试数据源

来源：Everlook `dbcformat/dbc.desc` + Mapache WDBXEditor + WoWDBDefs。

首批表：

```text
Map.dbc
AreaTable.dbc
LiquidType.dbc
CreatureModelData.dbc
CreatureDisplayInfo.dbc
GameObjectDisplayInfo.dbc
ItemDisplayInfo.dbc
```

优先级：**中高**。

---

## 7.4 RuntimeRegressionCatalog — 强烈推荐

来源：Everlook-Bugtracker。

不要复制大量 issue 正文；只记录：

```text
issue id
asset subsystem
symptom class
our regression test
```

示例：

```text
#342 -> ADT terrain visual -> seam/height/patch test
#512 -> WMO/VMAP LOS -> collision ray fixture
#77  -> texture dependency -> missing BLP manifest test
#21  -> display binding -> DBC/server binding test
```

优先级：**高**。

---

# 8. 不推荐的做法

1. 不要把 Wall-core 三个 server fork 全部 vendoring 进项目。
2. 不要把旧 Vanilla extractor 当 build12340 Source335 reader。
3. 不要因为 M2Workshop 有 Vanilla 支持，就把它视为 v264 -> v256 authoritative writer。
4. 不要直接复制 GPL-3.0 M2Workshop 大量实现到非 GPLv3 converter。
5. 不要把 gameplay bugtracker 问题全部误分类为 asset-format bug。
6. 不要把 VMaNGOS/Everlook/Ashen 的重复代码视为三份独立证据；先比较 blob SHA/实现差异。

---

# 9. 最终排名

| 仓库 | 对 Turtle335Converter 的价值 | 最适合用途 |
|---|---|---|
| `Wall-core/M2Workshop` | A | M2 repair、Vanilla兼容经验、回归/人工验证 |
| `Wall-core/VMaNGOS-Default` | A- | target extractor/vmap、legacy path resolver、GameObject模型映射 |
| `Wall-core/Everlook` | B+ | legacy DBC schema、extractor/vmap fork对照 |
| `Wall-core/AshenWoW` | B- | fork-specific extractor差异对照 |
| `Wall-core/Everlook-Bugtracker` | A-（验证维度） | runtime regression corpus |
| `Wall-core/Wallcraft-bugtracker` | D | 当前几乎无资产转换价值 |

---

# 10. 对当前开发路线的影响

Wall-core 调研没有推翻当前架构：

```text
3.3.5 source reader
  -> Normalized assets
  -> semantic downgrade
  -> Turtle/Vanilla writer
  -> client validator
  -> Penqle/Tortoise server extractor
  -> maps/vmaps/mmaps regression
```

它新增了四个值得落地的横向能力：

```text
LegacyAssetPathResolver
M2RepairValidator
LegacyDbcSchemaCrossCheck
RuntimeRegressionCatalog
```

这些能力应作为现有 ADT/M2/WMO 核心的辅助层，而不是替代目前已经用真实客户端 loader 建立的格式真值。
