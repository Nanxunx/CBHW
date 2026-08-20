# WMO Retroport Spec — 3.3.5a -> Vanilla/Turtle 1.18.1

状态：**Checkpoint-M2/WMO**

目标：把 3.3.5a WMO root/group 转换成 Vanilla 1.12.x / Turtle WoW 1.18.1 可安全加载的旧式 WMO，而不是只保持 `MVER=17`。

---

## 1. 核心结论

### 已确认

- Classic/TBC/WotLK WMO 都可能使用 `MVER=17`。
- 因此 WMO 版本号不能用于区分 Vanilla 与 WotLK 功能集合。
- Root WMO 多个记录尺寸在两代客户端之间保持不变。
- Group WMO 基础 geometry stride 基本一致。
- WotLK group 增加第二套 vertex-color / UV 的高位 flag。
- Turtle 旧 loader 没有对应的 WotLK second-set 路线。
- MOPY bit 语义在 3.3.5 与旧 Tortoise extractor 中不同。
- MLIQ 物理网格结构基本一致，主要差异在 liquid type 解释。

### 待继续确认

- 3.3.5 MOMT shader 0..6 到 Turtle runtime shader 的精确逐 ID 对应。
- 第二套 MOCV/MOTV 是否能对部分 shader 无损折叠。
- Turtle 自定义客户端是否对某些 WotLK shader 添加了额外兼容路径。

---

# 2. Root WMO

## 2.1 关键 chunks

```text
MVER
MOHD
MOTX
MOMT
MOGN
MOGI
MOSB
MOPV
MOPT
MOPR
MOVV
MOVB
MOLT
MODS
MODN
MODD
MFOG
MCVP (部分版本/对象)
```

## 2.2 已确认 stride

```text
MOMT = 64 B/material
MOGI = 32 B/group
MOLT = 48 B/light
MODS = 32 B/set
MODD = 40 B/doodad
MFOG = 48 B/fog
```

原则：**record stride 一样，只能证明物理读取边界兼容，不能证明字段语义兼容。**

---

## 2.3 MOHD

### 风险

3.3.5 与旧 Vanilla/Tortoise 代码中，对 MOHD 尾部字段的命名/解释不同。

转换器应：

1. 读取 source MOHD 到语义结构。
2. 不 raw-copy 尾部 flags/liquid-related value。
3. 根据目标客户端实际 loader 重新编码目标字段。

建议：

```cpp
struct NormalizedWmoHeader
{
    uint32_t materialCount;
    uint32_t groupCount;
    uint32_t portalCount;
    uint32_t lightCount;
    uint32_t doodadNameCount;
    uint32_t doodadDefCount;
    uint32_t doodadSetCount;

    uint32_t sourceFlags;
    uint32_t rootWmoId;

    Vec3 bboxMin;
    Vec3 bboxMax;

    // 不直接暴露 raw tail，改成语义字段
    WmoRootSemanticFlags semanticFlags;
};
```

---

# 3. MOMT

## 3.1 物理布局

已确认：

```text
MOMT stride = 0x40 = 64 bytes
```

旧客户端 loader 至少明确消费：

```text
+0x0C texture0 offset in MOTX
+0x18 texture1 offset in MOTX
```

3.3.5 loader 明确读取 `shader` 字段并根据多个 shader 类型分支。

所以：

> MOMT 不能因为 stride 相同就 raw-copy。

---

## 3.2 Normalized Material

推荐：

```cpp
struct NormalizedWmoMaterial
{
    uint32_t flags;
    uint32_t sourceShader;
    uint32_t blendMode;

    std::string texture0;
    std::string texture1;
    std::string texture2;

    Color color0;
    Color color1;
    Color color2;

    uint32_t groundType;

    bool usesSecondUv;
    bool usesSecondVertexColor;
};
```

---

## 3.3 Material downgrade tiers

### Tier A — 单纹理 / 单 UV

行为：

```text
Texture0 + MOTV1
```

策略：

- 直接映射到目标旧 shader。
- 清理源版本专属 flags。
- 保留 blend mode（前提是目标支持）。

### Tier B — 双纹理 / 单 UV

行为：

```text
Texture0 + Texture1 + MOTV1
```

策略：

- 保留两纹理。
- 尽可能映射到旧客户端双纹理路径。
- 必须运行验证。

### Tier C — 第二 UV / 第二 vertex color

行为：

```text
Texture0 + UV1
Texture1 + UV2
MOCV1 + MOCV2
```

策略优先级：

1. 已知可无损折叠：使用专门 mapper。
2. 可离线 bake：生成新 BLP + 单 UV。
3. 无法安全表达：降级到主要材质并产生 Warning。

绝对禁止静默删除第二套 UV/color 后仍声称“转换成功且无损”。

---

# 4. Group WMO

## 4.1 MOGP 基础 header

已确认：

```text
MOGP header = 68 bytes
```

## 4.2 基础子块 stride

```text
MOPY = 2 B/triangle
MOVI = 2 B/index
MOVT = 12 B/vertex
MONR = 12 B/normal
MOTV = 8 B/UV
MOBA = 24 B/batch
```

基础 geometry 可以保留数据并重写 chunk/header，而不是重采样 mesh。

---

# 5. WotLK second sets

3.3.5 loader 中已经确认：

```text
MOGP flag 0x01000000 -> second MOCV / CVERTS2
MOGP flag 0x02000000 -> second MOTV / TVERTS2
```

对应 stride：

```text
MOCV2 = 4 bytes / vertex
MOTV2 = 8 bytes / vertex
```

目标 Turtle 旧 loader 没有对应路径。

## 5.1 Flag handling

第一步：

```cpp
constexpr uint32_t WOTLK_CVERTS2 = 0x01000000;
constexpr uint32_t WOTLK_TVERTS2 = 0x02000000;

uint32_t targetFlags =
    sourceFlags & ~(WOTLK_CVERTS2 | WOTLK_TVERTS2);
```

但这只是最后编码步骤。

真正流程必须是：

```text
parse source group
  -> detect second sets
  -> inspect materials/batches
  -> downgrade/bake
  -> remove second-set data
  -> clear source-only flags
  -> write target group
```

---

# 6. MOCV downgrade

推荐接口：

```cpp
struct VertexColorDowngradeResult
{
    std::vector<Color32> targetColors;
    bool lossy;
    std::string warning;
};

VertexColorDowngradeResult ConvertVertexColors(
    Span<Color32> mocv1,
    Span<Color32> mocv2,
    const MaterialUsageGraph& usage);
```

### 第一版 policy

- 只有 MOCV1：直接保留。
- MOCV2 存在但没有任何目标 material 需要：丢弃 MOCV2，记录 informational log。
- MOCV2 参与 shader：进入 bake 或 explicit lossy downgrade。

---

# 7. MOTV downgrade

推荐接口：

```cpp
struct UvDowngradeResult
{
    std::vector<Vec2> targetUv;
    bool requiresTextureBake;
    bool lossy;
};
```

### 第一版 policy

- 单 UV：直接保留 MOTV1。
- 双 UV，但第二 UV 与第一 UV 完全相同：合并。
- 双 UV 且 shader 不再使用第二层：删除 MOTV2。
- 双 UV 且第二层必须保留：texture bake。

可增加优化：

```text
if MOTV2 ~= affine_transform(MOTV1)
```

则可尝试把 UV transform bake 到 texture，而不必重新展开 mesh。

---

# 8. MOPY semantic conversion

## 8.1 3.3.5 semantics

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

参考：TrinityCore `src/tools/vmap4_extractor/wmo.h`。

## 8.2 Old Tortoise semantics

```text
0x01 NOCAMCOLLIDE
0x02 DETAIL
0x04 NO_COLLISION
0x08 HINT
0x10 RENDER
0x20 COLLIDE_HIT
0x40 WALL_SURFACE
```

参考：Penqle/tortoise-wow `tools/vmap_extractor/vmapextract/wmo.h`。

## 8.3 禁止机械位移

错误：

```cpp
targetFlags = sourceFlags >> 1;
```

正确：

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
};
```

读取 source -> semantics -> target encoding。

---

## 8.4 Collision preservation

3.3.5 extractor 的核心逻辑：

```cpp
bool isRenderFace = render && !detail;
bool isCollision = collision || isRenderFace;
```

旧 Tortoise extractor 依据 `NO_COLLISION` 与 `HINT/COLLIDE_HIT` 组合筛选。

转换时必须保证：

```text
Source final collision semantics
        ==
Target extractor final collision semantics
```

建议：

```cpp
bool sourceCollision =
       s.collision
    || (s.render && !s.detail);

if (!sourceCollision)
{
    target |= VANILLA_NO_COLLISION;
}
else
{
    if (!s.hint && !s.collideHit)
        target |= VANILLA_COLLIDE_HIT;
}
```

目标不是保存原始 bits，而是保存游戏行为。

---

# 9. MLIQ

## 9.1 Physical layout

当前已确认两边基本一致：

```text
MLIQ header = 30 bytes
liquid vertices = 8 bytes * xverts * yverts
liquid tiles = 1 byte * xtiles * ytiles
```

所以第一版不重建 liquid geometry。

## 9.2 Semantic conversion

需要统一转换：

```text
3.3.5 liquid type / flags
        ->
normalized liquid category
        ->
Vanilla/Turtle liquid id/category
```

Normalized enum 建议：

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
```

与 ADT `MH2O -> MCLQ` 共用 `LiquidTypeMapper`。

---

# 10. Doodad references

Root WMO：

```text
MODN -> doodad M2 paths
MODD -> placement records
MODS -> sets
```

转换 WMO 时必须把 `MODN` 中引用的 M2 路径加入资源依赖图：

```text
WMO
  -> doodad M2
      -> BLP / skin / animation dependencies
```

如果 M2 被输出到新路径，必须同步重写 MODN/MODD 对应引用。

---

# 11. Writer rules

Writer 不应复用 source chunk offsets。

正确流程：

```text
serialize chunk payload
  -> compute payload size
  -> write FOURCC
  -> write size
  -> write payload
```

Group 内部同样重新构造所有子块。

对未识别 source chunk：

- 默认不要 blind-copy 到 Vanilla target。
- 先进入 diagnostics。
- 只有确认目标 loader 忽略且不会改变 chunk-order parser 时才允许保留。

---

# 12. WMO validator

至少检查：

- MVER
- root chunk boundaries
- MOMT count * 64
- MOGI count * 32
- MODS count * 32
- MODD count * 40
- group MOGP header 68B
- MOPY / MOVI / MOVT / MONR / MOTV / MOBA strides
- MOVI indices < vertex count
- MOPY triangle count matches MOVI/3
- MOBA ranges inside MOVI
- texture offsets point inside MOTX and begin at valid string
- MODD name offsets point inside MODN
- MOCV count == vertex count
- MOTV count == vertex count
- target flags do not claim missing chunks
- MLIQ dimensions do not overflow chunk

---

# 13. Source references

- TrinityCore 3.3.5 WMO extractor:
  - https://github.com/TrinityCore/TrinityCore/blob/3.3.5/src/tools/vmap4_extractor/wmo.h
  - https://github.com/TrinityCore/TrinityCore/blob/3.3.5/src/tools/vmap4_extractor/wmo.cpp
- Penqle/tortoise-wow old extractor:
  - https://github.com/Penqle/tortoise-wow/blob/main/tools/vmap_extractor/vmapextract/wmo.h
  - https://github.com/Penqle/tortoise-wow/blob/main/tools/vmap_extractor/vmapextract/wmo.cpp
- warcraft-rs WMO parser/converter:
  - https://github.com/wowemulation-dev/warcraft-rs/tree/main/file-formats/graphics/wow-wmo
- Vanilla client internals research:
  - https://github.com/samwhosung/wow-1121-client-internals

最终裁判仍然是：**目标 Turtle 客户端 loader + 目标 Tortoise extractor + 实机运行测试。**
