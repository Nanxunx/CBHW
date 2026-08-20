# Turtle335Converter 研究 Checkpoint

> 项目目标：将 WoW 3.3.5a 客户端资源可靠地 retroport 到 Vanilla 1.12.x / Turtle WoW 1.18.1（build 7272）可加载格式。
>
> 当前推荐路线：`3.3.5a assets -> Vanilla/1.12-compatible assets -> Turtle WoW 1.18.1`。
>
> 本文用于保存本项目当前已经分析出的结论。任何后续实现都应优先继承这里的证据，不要重新退回“只改版本号”“删除未知块”“原样复制高版本结构”等已经被否定的方案。

## 1. 证据等级

研究结果统一分为四级：

1. **客户端二进制确认**：来自实际 Turtle 1.18.1 / WoW 3.3.5a `WoW.exe` 的静态逆向。
2. **服务端/工具源码确认**：来自 TrinityCore 3.3.5、Penqle/tortoise-wow extractor 等源码。
3. **强推断**：二进制、格式结构和多个源码实现相互吻合，但还缺最终运行测试。
4. **未验证**：只能作为调查方向，禁止直接写进 converter 的不可逆转换逻辑。

核心原则：**尺寸一致不等于语义一致；版本号一致也不等于功能集合一致。**

---

## 2. Turtle 1.18.1 客户端基线

已分析的 Turtle 客户端 `WoW.exe` 明确保留 Vanilla 1.12.1 build 5875 基础，同时加入 Turtle 自定义版本标识：

- PE32 / x86
- VERSIONINFO 仍出现 `1, 12, 1, 5875`
- 字符串包含 `WoW [Release] Build 5875`
- 同时包含 `7272` 与 `1.18.1`
- 保留 Storm/MPQ 资源系统
- 保留 `MD20` M2 相关加载路径

因此当前工程不应把 Turtle 1.18.1 当成 1.14 Classic/CASC 客户端处理。

### 尚不能仅凭版本字符串证明的事项

- Turtle 是否增加了部分 WotLK ADT/M2/WMO loader 扩展
- 所有 M2 子结构的最终接受版本范围
- MH2O 是否存在额外兼容路径
- WMO 某些高版本 shader 是否被 Turtle 自定义支持

这些必须继续以 loader 机器码或实际资源运行测试确认。

---

## 3. 总体转换架构

推荐 converter 使用中间语义模型，而不是做“字节偏移修补器”：

```text
3.3.5 Reader
    |
    v
Normalized Asset Model
    |
    +-- M2Retroporter
    +-- WmoRetroporter
    +-- AdtRetroporter
    +-- LiquidTypeMapper
    +-- BlpConverter / MaterialBaker
    +-- DbcReferenceMapper
    |
    v
Vanilla/Turtle Writer
    |
    v
Validator
```

转换器必须做到：

- parse -> normalize semantics -> convert -> rebuild offsets -> write -> validate
- 不能依靠原始 offset 在删块/插块后继续有效
- 对无法无损表达的高版本功能，必须发 Warning 或执行明确的 downgrade policy

---

# 4. M2 当前结论

## 4.1 目标方向

当前项目工作基线为：

```text
WoW 3.3.5a MD20/M2
        |
        v
Vanilla/Turtle-compatible MD20
```

项目先前对 Turtle/Vanilla loader 的逆向已经确认其 M2 仍属于旧式 MD20 runtime 路线。当前实现设计应以 **结构级转换** 为核心，而不是单纯修改 MVER/version integer。

## 4.2 关键转换对象

M2 retroport 至少必须处理：

- header element arrays / counts / offsets
- sequences / animations
- bones
- vertices
- colors
- textures and texture lookup
- texture transforms
- render flags / materials
- attachment
- camera
- lights
- ribbons
- particles
- skin profile / views

尤其是 ribbon / particle / skin 相关结构，是版本差异最大的区域之一，不允许简单截断。

## 4.3 Turtle / Vanilla M2 loader 已确认特征

旧客户端逆向中已经恢复出多组 runtime stride，例如：

- bone runtime block
- color block
- texture transform block
- light file/runtime layout
- camera file layout
- texture record
- attachment
- ribbon
- particle

这些证据说明目标 writer 必须输出旧 loader 实际消费的结构，而不是只保证文件开头能通过版本检查。

## 4.4 M2 实现建议

```text
M2Reader335
  -> NormalizedM2
      -> HeaderConverter
      -> AnimationConverter
      -> SkinProfileConverter
      -> MaterialConverter
      -> RibbonConverter
      -> ParticleConverter
  -> VanillaM2Writer
  -> M2Validator
```

Validator 应至少检查：

- 所有 array offset + count 是否落在文件范围
- stride 是否匹配目标格式
- texture strings 是否有效
- skin index / vertex lookup 是否越界
- bone / animation lookup 是否越界
- attachment / camera / particle / ribbon 内部数组是否有效

---

# 5. WMO 当前结论

WMO 是目前证据最完整的部分之一。

## 5.1 WMO version

Classic / TBC / WotLK 的 WMO 文件常见 `MVER = 17`。这意味着：

> **WMO 不能依赖 MVER=17 判断“这是 Vanilla 还是 WotLK”。**

真正差异更多来自：

- chunk presence
- MOGP flags
- material shader semantics
- liquid interpretation
- second UV / second vertex-color sets

## 5.2 Root WMO 物理尺寸

Turtle/Vanilla 与 3.3.5 的大量 root record 尺寸保持一致，例如当前已验证的重要记录：

```text
MOMT = 64 bytes/material
MOGI = 32 bytes/group
MOLT = 48 bytes/light
MODS = 32 bytes/set
MODD = 40 bytes/doodad
MFOG = 48 bytes/fog
```

但必须牢记：**相同尺寸中的字段语义可能不同。**

### MOHD 尾部 4 字节

一个关键例子：

- TrinityCore 3.3.5 extractor 把 MOHD 尾部 4 字节作为 `flags`
- 旧 Tortoise extractor 中相应字段历史上按 `liquidType` 路线使用

因此 MOHD 不能 raw-copy 后就认为语义完全兼容。

---

## 5.3 Group WMO 基础结构

两边已确认：

```text
MOGP header = 68 bytes
MOPY = 2 bytes/triangle
MOVI = 2 bytes/index
MOVT = 12 bytes/vertex
MONR = 12 bytes/normal
MOTV = 8 bytes/UV
MOBA = 24 bytes/batch
```

所以基础 geometry 本身具备很好的 retroport 条件。

---

## 5.4 3.3.5 高位 MOGP 扩展

3.3.5 客户端真实 loader 中已经确认存在两类 Turtle 旧 loader 没有对应处理的高位扩展：

```text
MOGP flag 0x01000000 -> 第二套 MOCV（CVERTS2）
MOGP flag 0x02000000 -> 第二套 MOTV（TVERTS2）
```

对应元素尺寸：

```text
MOCV2 = 4 bytes / vertex
MOTV2 = 8 bytes / vertex
```

因此不能把带这两个 flag 的 3.3.5 group WMO 原样复制给目标客户端。

第一版 Group flag 降级至少需要：

```cpp
constexpr uint32_t WOTLK_CVERTS2 = 0x01000000;
constexpr uint32_t WOTLK_TVERTS2 = 0x02000000;

targetFlags = sourceFlags & ~(WOTLK_CVERTS2 | WOTLK_TVERTS2);
```

但必须先处理对应数据，**禁止只清 flag 不处理 MOCV2/MOTV2。**

---

## 5.5 MOCV2 降级

WotLK WMO 可能拥有：

```text
MOCV #1
MOCV #2
```

目标旧格式只可靠支持单套顶点颜色时，需要：

```text
MOCV1 + MOCV2
    -> VertexColorRetroporter
    -> one Vanilla-compatible MOCV
```

第一版策略：

1. 普通材质优先保留 MOCV1。
2. 若材质/batch 明确依赖第二套 color，则进入高级降级。
3. 无法安全判断时必须 Warning，不得静默丢失。

最终高质量方案是根据材质与 batch 语义进行 color bake。

---

## 5.6 MOTV2 / 第二套 UV 降级

WotLK 可出现：

```text
MOTV1 = UV set 0
MOTV2 = UV set 1
```

Vanilla/Turtle 旧 loader 只有单套基础 MOTV 路线时，第二套 UV 不能简单删除。

如果第二层纹理使用 MOTV2，直接删除会导致：

- 纹理拉伸
- UV 错位
- tiling 比例错误
- 多层材质表现错误

因此需要 `MultiUvRetroporter`。

对于无法用旧 shader 表达的材质，推荐最终使用 **texture bake**：

```text
Texture A + UV1
Texture B + UV2
MOCV1/MOCV2
      |
      v
Offline Material Bake
      |
      v
new Vanilla-compatible BLP + single UV
```

---

## 5.7 MOMT 材质

两边真实 loader 都确认：

```text
MOMT stride = 64 bytes
```

Vanilla/Turtle 旧 loader 明确解析两条主要纹理路径：

```text
+0x0C -> texture 0
+0x18 -> texture 1
```

3.3.5 loader 会读取 shader 字段，并针对多个 shader 类型走不同路径。

因此：

> **MOMT 记录尺寸可兼容，不代表 shader 行为兼容。**

建议材质 retroport 分三级：

### Tier A：单纹理 / 单 UV

通常可以直接降级到旧式 Diffuse/Opaque 等路径。

### Tier B：双纹理 / 单 UV

保留两纹理与 MOTV1，再做目标客户端实机验证。

### Tier C：第二 UV / 第二 color set

需要 texture/color bake，或者明确降级到单层材质并产生 Warning。

---

## 5.8 MOPY：必须按语义转换，不能 bit-shift

### Trinity 3.3.5 MOPY 语义

```text
0x01 unknown
0x02 NOCAMCOLLIDE
0x04 DETAIL
0x08 COLLISION
0x10 HINT
0x20 RENDER
0x40 WALL_SURFACE
0x80 COLLIDE_HIT
```

### 旧 Tortoise 语义

```text
0x01 NOCAMCOLLIDE
0x02 DETAIL
0x04 NO_COLLISION
0x08 HINT
0x10 RENDER
0x20 COLLIDE_HIT
0x40 WALL_SURFACE
```

因此这种实现是错误的：

```cpp
target = source >> 1; // WRONG
```

正确做法是先归一化：

```cpp
struct WmoTriangleSemantic
{
    bool noCameraCollision;
    bool detail;
    bool collision;
    bool hint;
    bool render;
    bool wall;
    bool collideHit;
    uint8_t material;
};
```

然后根据目标编码重新生成 bits。

### 碰撞补偿

Trinity 3.3.5 的碰撞判断近似：

```cpp
isRenderFace = RENDER && !DETAIL;
isCollision  = COLLISION || isRenderFace;
```

旧 Tortoise extractor 则会基于 `NO_COLLISION` 与 `HINT/COLLIDE_HIT` 组合筛面。

因此转换时应保留“源文件最终碰撞语义”，必要时给目标 MOPY 补一个可被旧 extractor 接受的碰撞 bit，而不是机械复制。

---

## 5.9 MLIQ

当前确认的物理布局在两边基本一致：

```text
header = 30 bytes
vertices = 8 bytes * xverts * yverts
tile bytes = 1 byte * xtiles * ytiles
```

所以 WMO liquid 的主要问题不是重建 geometry，而是：

```text
Liquid type / category mapping
```

建议与 ADT 液体转换共用：

```text
LiquidTypeMapper
```

将 WotLK 的 LiquidType/flags 语义降级到旧客户端可表达的 water/ocean/magma/slime 等类别。

---

# 6. WMO 第一版兼容矩阵

| 部分 | 3.3.5 -> Turtle | 当前处理 |
|---|---|---|
| MVER 17 | 基本兼容 | 保留 |
| MOHD | 尺寸近似兼容 | 尾字段/flags 重新解释 |
| MOTX | 兼容 | 路径验证 |
| MOMT 64B | 尺寸兼容 | shader/material 降级 |
| MOGN | 兼容 | 保留 |
| MOGI 32B | 兼容 | flags 检查 |
| MOSB | 基本兼容 | 条件保留 |
| MOPV/MOPT/MOPR | 基本兼容 | 保留 |
| MOLT 48B | 尺寸兼容 | 灯光语义验证 |
| MODS 32B | 兼容 | 保留 |
| MODN | 结构兼容 | 引用转换后的 M2 |
| MODD 40B | 兼容 | 保留 placement |
| MFOG 48B | 基本兼容 | 验证 |
| MOGP 68B | 尺寸兼容 | flags 降级 |
| MOPY 2B | 尺寸相同 | **语义重编码** |
| MOVI | 兼容 | 保留 |
| MOVT | 兼容 | 保留 |
| MONR | 兼容 | 保留 |
| MOTV1 | 兼容 | 保留 |
| MOTV2 | 目标旧 loader 不支持 | 降级 / Bake |
| MOBA 24B | 兼容 | material 验证 |
| MOCV1 | 兼容 | 保留 |
| MOCV2 | 目标旧 loader 不支持 | 合并 / 选择 / Bake |
| MOLR/MODR | 基本兼容 | 保留并修正引用 |
| MOBN/MOBR | 基本兼容 | 保留 BSP |
| MLIQ | 物理布局兼容 | LiquidType 重映射 |

---

# 7. ADT 当前结论

ADT 是地图转换中最重要的资源容器之一。

## 7.1 Vanilla 常见核心 chunks

```text
MVER
MHDR
MCIN
MTEX
MMDX
MMID
MWMO
MWID
MDDF
MODF
MCNK x 256
```

MCNK 内部承载：

- height
- normals
- layers
- alpha maps
- doodad/WMO refs
- shadow
- holes
- liquid
- flags / offsets

## 7.2 3.3.5 -> Vanilla 的关键液体差异

高价值转换点是：

```text
WotLK MH2O
    ->
Vanilla-style MCLQ
```

这不是“删除 MH2O”问题，而是需要：

- parse MH2O
- 按 MCNK 生成目标 MCLQ
- 重新生成 liquid flags
- 重算 MCNK 内部 offsets
- 重写 MCNK header

## 7.3 ADT 其他必须验证/转换的内容

- MCAL alpha map encoding
- MCSH
- holes
- texture layers
- MMDX/MMID
- MWMO/MWID
- MDDF / MODF placement
- object references
- MCNK subchunk offsets
- WDT / WDL 关联

推荐实现：

```text
Adt335Reader
  -> NormalizedAdt
      -> TerrainConverter
      -> TextureLayerConverter
      -> Mh2oToMclqConverter
      -> PlacementConverter
      -> ReferenceTableRebuilder
  -> VanillaAdtWriter
  -> AdtValidator
```

---

# 8. DBC 与引用关系

资源转换不能只处理二进制文件本身。至少需要考虑：

- Map.dbc
- AreaTable.dbc
- GameObjectDisplayInfo.dbc
- CreatureDisplayInfo.dbc
- CreatureModelData.dbc
- ItemDisplayInfo.dbc
- LiquidType/相关液体表（按目标客户端实际表结构决定）

需要建立：

```text
Source asset path / display ID
      ->
Converted asset path / target ID
```

否则资源虽然能打开，服务器/客户端仍可能引用到错误路径或不存在的 display。

---

# 9. 推荐代码目录

```text
src/
  core/
    BinaryReader.*
    BinaryWriter.*
    ChunkReader.*
    Diagnostics.*

  m2/
    M2Reader335.*
    M2Normalized.*
    M2Retroporter.*
    M2WriterVanilla.*
    M2Validator.*

  wmo/
    WmoReader335.*
    WmoNormalized.*
    WmoMaterialRetroporter.*
    WmoMopyRetroporter.*
    WmoLiquidRetroporter.*
    WmoWriterVanilla.*
    WmoValidator.*

  adt/
    AdtReader335.*
    AdtNormalized.*
    Mh2oToMclq.*
    AdtRetroporter.*
    AdtWriterVanilla.*
    AdtValidator.*

  blp/
    BlpReader.*
    BlpConverter.*
    MaterialBaker.*

  dbc/
    DbcMapper.*

  cli/
    main.*
```

---

# 10. Validation 设计

转换完成后至少经过四层验证。

## Level 1 - Binary validation

- chunk 边界
- count/stride
- offsets
- string offsets
- array ranges

## Level 2 - Target parser validation

尽可能用 Vanilla/Tortoise 对应 parser/extractor 对输出文件重新 parse。

## Level 3 - Server asset extraction

使用目标服务端工具链生成：

```text
maps
vmaps
mmaps
```

如果 extractor 已经失败，则不进入客户端实机测试。

## Level 4 - Turtle 1.18.1 runtime test

检查：

- client crash
- missing texture
- pink/white materials
- UV distortion
- invisible WMO
- collision loss
- water type/height
- doodad orientation
- M2 animation
- particle/ribbon effects

---

# 11. 当前优先级

推荐继续顺序：

1. **MOMT shader 0..6 -> Turtle/Vanilla 实际 shader 的机器码级映射**
2. 完成 `MOPY semantic mapper`
3. 完成 `MOGP flag downgrade`
4. 完成 `MOTV2/MOCV2` 策略
5. 完成 WMO liquid mapper
6. 冻结 WMO v1 converter spec
7. 回到 ADT，完成 `MH2O -> MCLQ`
8. 把 M2 + WMO + ADT 串成批量转换流程

最终目标：

```text
335 ADT
 |- terrain
 |- WMO placements
 |    `- WMO
 |        `- doodad M2
 `- liquids

       -> Turtle335Converter ->

Turtle-compatible ADT / WMO / M2 / BLP
       ->
Tortoise extractor
       ->
maps / vmaps / mmaps
       ->
Turtle WoW 1.18.1 runtime
```

---

# 12. 重要源码参考

- Penqle/tortoise-wow: https://github.com/Penqle/tortoise-wow
- TrinityCore 3.3.5: https://github.com/TrinityCore/TrinityCore/tree/3.3.5
- warcraft-rs: https://github.com/wowemulation-dev/warcraft-rs
- Vanilla client internals research: https://github.com/samwhosung/wow-1121-client-internals

具体实现时应继续以真实客户端 loader + 目标 extractor 为最终裁判，不应只依赖 wiki 或第三方 struct 名称。

---

## 13. 当前状态

**Checkpoint-M2/WMO**

- Turtle 1.18.1 = Vanilla-derived MPQ/MD20 client：已确认。
- WMO root/group 大量物理 stride：已确认。
- MOGP second UV / second color high flags：已确认。
- MOPY 两代 bit 语义不同：已确认。
- MLIQ physical grid layout 基本一致：已确认。
- WMO shader 的精确逐 ID downgrade：正在继续逆向。
- ADT MH2O -> MCLQ：转换方向已确定，字段级实现仍需继续冻结。
- M2 完整 writer：需要继续把已恢复 loader 结构整理为正式 spec 与代码。

此文档应随着逆向结果持续更新，而不是覆盖掉历史证据。
