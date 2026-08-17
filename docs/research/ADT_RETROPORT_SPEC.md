# ADT Retroport Spec — 3.3.5a -> Vanilla/Turtle 1.18.1

状态：研究中。

目标：把 WoW 3.3.5a 地图 ADT/WDT/WDL 及其资源引用转换成 Vanilla 1.12.x / Turtle WoW 1.18.1 可加载格式，并保证 Tortoise extractor 能继续生成 maps/vmaps/mmaps。

---

## 1. 设计原则

ADT 不能按“删掉高版本 chunk”处理。

必须：

```text
parse source
 -> normalize terrain/liquid/placements/layers
 -> convert semantics
 -> rebuild MCNK internals
 -> rebuild top-level offsets/tables
 -> write target
 -> validate
```

所有 MCNK 内部 offsets 必须在写出时重新计算。

---

# 2. Vanilla 常见顶层 chunks

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

其中：

- MTEX：地表纹理路径
- MMDX/MMID：M2 模型路径及 offset 表
- MWMO/MWID：WMO 路径及 offset 表
- MDDF：M2 placement
- MODF：WMO placement
- MCNK：16 x 16 terrain chunks

---

# 3. Normalized ADT

建议：

```cpp
struct NormalizedAdt
{
    uint32_t version;

    std::vector<std::string> textures;
    std::vector<std::string> m2Names;
    std::vector<std::string> wmoNames;

    std::vector<M2Placement> m2Placements;
    std::vector<WmoPlacement> wmoPlacements;

    std::array<NormalizedMcnk, 256> chunks;
};
```

MCNK：

```cpp
struct NormalizedMcnk
{
    int indexX;
    int indexY;

    uint32_t sourceFlags;

    std::vector<float> heights;
    std::vector<Vec3> normals;

    std::vector<TerrainLayer> layers;
    std::vector<AlphaMap> alphaMaps;

    ShadowMap shadow;
    HoleMask holes;

    std::vector<uint32_t> doodadRefs;
    std::vector<uint32_t> wmoRefs;

    NormalizedLiquid liquid;
};
```

---

# 4. 最大转换点：MH2O -> MCLQ

3.3.5 地图常见新式液体系统：

```text
MH2O
```

Vanilla 旧地图使用：

```text
MCLQ
```

正确转换不是：

```text
remove MH2O
```

而是：

```text
MH2O
 -> parse each MCNK liquid layer
 -> normalize liquid surfaces
 -> choose target legacy liquid category
 -> generate MCLQ payload
 -> update MCNK liquid flags
 -> rebuild MCNK offsets
```

---

# 5. Normalized liquid

ADT 与 WMO 应共用同一个语义层：

```cpp
enum class LiquidCategory
{
    None,
    Water,
    Ocean,
    Magma,
    Slime,
    Unknown
};

struct NormalizedLiquid
{
    LiquidCategory category;

    int minX;
    int minY;
    int width;
    int height;

    std::vector<float> heights;
    std::vector<uint8_t> tileFlags;

    uint32_t sourceLiquidType;
};
```

然后：

```text
ADT MH2O
WMO MLIQ
     |
     v
LiquidTypeMapper
     |
     v
Vanilla/Turtle liquid semantics
```

---

# 6. MCLQ generator

第一版 generator 需要完成：

1. 根据 MH2O 有效区域确定 legacy liquid grid。
2. 重建每个 liquid vertex 高度。
3. 把 hole/empty tile 转成 target tile flags。
4. 根据 LiquidType 映射 water/ocean/magma/slime。
5. 生成 MCLQ。
6. 更新 MCNK header 中 liquid 相关 flag / size / offset。

必须对：

- 多层液体
- 部分 tile 液体
- 洞穴内液体
- 海洋边界

给出明确 downgrade policy。

如果 target MCLQ 无法表示 source 多层液体：

- 选择 gameplay-visible 主层
- 产生 lossy warning
- 不得静默丢失

---

# 7. MCNK

MCNK 是 ADT writer 的核心。

需要处理的典型子块：

```text
MCVT - heights
MCNR - normals
MCLY - texture layers
MCRF - model/WMO refs
MCSH - shadows
MCAL - alpha maps
MCLQ - legacy liquid
```

不同版本可能还包含额外/不同子块。

## Writer 规则

每个 MCNK：

```text
build MCVT
build MCNR
build MCLY
build MCRF
build MCSH if needed
build MCAL
build MCLQ if liquid

calculate every offset
write MCNK header
write subchunks
```

禁止继承 source `ofsMCVT/ofsMCNR/...`。

---

# 8. Heights / normals

### Heights

尽量直接保留 terrain vertex heights，不做重采样。

Validator：

- vertex count 符合 target MCNK 格式
- NaN/Inf 禁止
- terrain base position + relative height 不溢出

### Normals

如果两代法线编码不同：

```text
source packed normal
 -> float Vec3
 -> normalize
 -> target packed normal
```

不要对 packed bytes 做 bit-level 猜测复制。

---

# 9. MCLY / MCAL

Texture layer conversion 必须同时看：

```text
MCLY layer flags
MCAL alpha payload
MTEX texture table
```

不能只转换 MCLY。

需要确认：

- alpha map compression
- high-res / low-res alpha layout
- layer count
- texture index
- effect/flag fields

建议 normalized layer：

```cpp
struct TerrainLayer
{
    uint32_t textureIndex;
    uint32_t semanticFlags;
    int effectId;
};
```

Alpha map：

```cpp
struct AlphaMap
{
    int width;
    int height;
    std::vector<uint8_t> alpha;
};
```

先解码成统一 0..255，再重新编码 target MCAL。

---

# 10. MCSH / holes

Shadow 和 terrain holes 都属于容易出现“地图能加载但视觉/碰撞错误”的区域。

策略：

- MCSH：先 normalize bitmask，再写 target。
- holes：确认 source/target hole mask 粒度与位定义。
- 不要只复制 raw flag word。

---

# 11. Resource name tables

## M2

```text
MMDX = zero-terminated path blob
MMID = offsets into MMDX
```

## WMO

```text
MWMO = zero-terminated path blob
MWID = offsets into MWMO
```

Writer 应从 normalized string vector **重新生成整个 string block 与 offset table**。

这样可以安全支持：

- 路径规范化
- 模型输出到新路径
- 去重
- extension 修正

---

# 12. MDDF / MODF

ADT placement 与实际转换后的 M2/WMO 必须同步。

### MDDF

至少归一化：

```text
name/reference index
unique id
position
rotation
scale
flags
```

### MODF

至少归一化：

```text
name/reference index
unique id
position
rotation
bounds
doodad set
name set
flags
```

转换目标：

```text
source placement semantics
 -> normalized placement
 -> target record
```

不要只 memcpy，因为不同客户端/工具可能对 flags 或 scale 字段解释不同。

---

# 13. Dependency graph

一个 ADT conversion job 必须先建立完整依赖：

```text
ADT
 |- MTEX -> BLP
 |- MMDX/MMID -> M2
 |                `- BLP/SKIN/other M2 deps
 |- MWMO/MWID -> WMO
 |                `- doodad M2 -> BLP
 |- MDDF placements
 `- MODF placements
```

Converter 的批处理单元最好不是“单文件”，而是：

```text
Map Asset Graph
```

这样才能保证路径与引用同步。

---

# 14. WDT / WDL

完整地图导入不能只处理 ADT。

需要继续调查：

- WDT map/tile flags
- global WMO references
- tile presence table
- WDL low-res height / horizon data

第一版如果只转换单个已有 ADT，可先把 WDT/WDL 作为独立阶段；但完整地图转换最终必须支持。

---

# 15. ADT validator

## Top-level

- required chunks present
- all chunk boundaries valid
- exactly expected MCNK count for target format
- MCIN points to valid MCNKs（若目标 writer 使用 MCIN）

## Name tables

- MMID offsets inside MMDX
- MWID offsets inside MWMO
- all paths zero-terminated

## Placements

- MDDF model index valid
- MODF WMO index valid

## MCNK

- header offsets inside MCNK
- MCVT count correct
- MCNR count correct
- MCLY layer count valid
- MCAL payload matches layers
- MCRF references valid
- MCLQ payload matches liquid dimensions

---

# 16. 目标 extractor validation

生成 target ADT 后：

```text
Tortoise map extractor
Tortoise vmap extractor
mmap generator
```

必须全部能处理输出资源。

如果 extractor 失败，不进入 Turtle client runtime test。

---

# 17. Runtime test matrix

第一轮地图测试建议覆盖：

1. 无液体普通平地。
2. 多 texture layer 地形。
3. 有小湖的地形。
4. 海岸 / ocean。
5. magma/slime。
6. 同时包含 M2 + WMO placements。
7. WMO 内部还有 doodad M2。
8. terrain holes。

检查：

```text
terrain height
texture alpha
holes
liquid height/type
M2 position/orientation
WMO position/orientation
collision
LOS
VMAP/MMAP extraction
```

---

# 18. 当前实现优先级

1. 从 3.3.5 source/extractor 冻结 MH2O 读法。
2. 从 Turtle/Vanilla target loader/extractor 冻结 MCLQ 目标结构。
3. 写 `NormalizedLiquid`。
4. 写 `Mh2oToMclq`。
5. 写 MCNK offset rebuilder。
6. 再处理 MCAL/MCLY。
7. 最后串 resource tables + placements。

---

# 19. Source references

- Penqle/tortoise-wow:
  - https://github.com/Penqle/tortoise-wow
- TrinityCore 3.3.5:
  - https://github.com/TrinityCore/TrinityCore/tree/3.3.5
- warcraft-rs:
  - https://github.com/wowemulation-dev/warcraft-rs
- WoWDev community format documentation should be used as secondary reference only; target loader/extractor remains final authority.
