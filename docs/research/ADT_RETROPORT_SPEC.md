# ADT Retroport Spec — 3.3.5a -> Vanilla/Turtle 1.18.1

状态：**第一版转换规则基本冻结；核心高风险项已有真实客户端/源码证据。**

目标：把 WoW 3.3.5a build 12340 的 ADT/WDT/WDL 及引用资源转换为 Turtle WoW 1.18.1 / Vanilla-derived 客户端可加载格式，同时保证 Penqle/tortoise-wow tools 能生成 maps/vmaps/mmaps。

详细证据文档：

- `ADT_TURTLE_CLIENT_BINARY_ANALYSIS.md`
- `ADT_MCAL_BINARY_ANALYSIS.md`
- `ADT_HOLES_BINARY_ANALYSIS.md`
- `WDT_WDL_BINARY_ANALYSIS.md`

---

## 1. 总设计原则

禁止把 ADT 转换实现成“改 MVER / 删除高版本 chunk / 原地修几个 offset”。

正式流程：

```text
3.3.5a ADT
    -> WotlkAdtReader
    -> NormalizedADT
    -> semantic downgrade
    -> VanillaAdtWriter
    -> reopen + validate
    -> Turtle client test
```

服务器输出是另一条路线：

```text
Original 3.3.5a ADT
    -> TC/AZ-style terrain + MH2O reader
    -> normalized server terrain/liquid
    -> Tortoise .map writer
    -> Tortoise vmap/mmaps pipeline
```

不要为了生成服务器 `.map` 先做一次 MH2O→MCLQ。

---

## 2. 目标客户端格式权威来源

优先级：

1. 用户提供的 Turtle WoW 1.18.1 `WoW.exe` 真客户端 loader。
2. Vanilla 1.12.1 客户端逆向交叉验证。
3. Penqle/tortoise-wow extractor。
4. 社区格式文档。

已确认 Turtle terrain subsystem 与 Vanilla loader 高度同源：

```text
0x6C2010  ADT root / MHDR offset-table parse
0x6AF970  MCNK legacy subchunk pointer fixup
0x68D540  MCLQ 9x9 liquid consumer
```

因此 Turtle 客户端目标应按 Vanilla legacy terrain representation 写出。

---

## 3. Normalized ADT

中间模型不得保留源文件二进制 offsets。

```cpp
struct NormalizedAdt
{
    std::vector<std::string> textures;
    std::vector<std::string> m2Names;
    std::vector<std::string> wmoNames;

    std::vector<M2Placement> m2Placements;
    std::vector<WmoPlacement> wmoPlacements;

    std::array<NormalizedMcnk, 256> chunks;
};
```

```cpp
struct NormalizedMcnk
{
    uint32_t indexX;
    uint32_t indexY;
    uint32_t sourceFlags;
    uint32_t areaId;

    std::array<float, 145> heights;
    std::array<Vec3, 145> normals;

    std::vector<NormalizedTextureLayer> layers;
    NormalizedShadowMap shadow;
    NormalizedHoleData holes;

    std::vector<uint32_t> doodadRefs;
    std::vector<uint32_t> wmoRefs;

    std::vector<NormalizedLiquidLayer> liquids;
};
```

Offsets 如：

```text
ofsMCVT
offsMCNR
offsMCLY
offsMCRF
offsMCAL
offsMCSH
offsMCLQ
```

只存在于 Source Reader / Target Writer，不进入 normalized model。

---

## 4. 顶层 chunks

目标普通 tile ADT 使用/重建：

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

可选 legacy chunks 根据真实输入/目标支持情况写出，例如：

```text
MFBO
```

WotLK-only 或目标不消费的 top-level metadata 不得盲目复制。

所有顶层 offset tables 在最终布局确定后重新生成。

---

## 5. MCNK Header — 字段级读取/写出

不要用同一个 C struct 解释两代文件。

源 build12340 在一些 server extractors 中可见 `uint32 holes` 声明，但真实客户端已证明该4字节区域实际应拆成：

```text
+0x3C uint16 holesLowRes
+0x3E uint16 other/legacy field
```

目标同样以：

```text
uint16 holes
uint16 pad/legacy
```

写出。

所以禁止：

```cpp
reinterpret_cast<VanillaMcnkHeader*>(&wotlkHeader)
```

必须逐字段构造目标 header。

---

## 6. MCVT / terrain heights

MCVT terrain topology在目标范围内保持同类 145-height staggered grid。

原则：

- 不做无意义重采样。
- 保留 terrain shape。
- 所有 float 必须 finite。
- shared boundaries 进行 seam validation。

Normalized representation统一使用 world/relative height语义，不携带文件地址。

---

## 7. MCNR / normals

第一版：

```text
source packed normal
 -> normalized Vec3
 -> normalize vector
 -> target packed normal
```

即使物理宽度相同，也不要把法线编码兼容性建立在 raw byte 假设上。

Validator 应检查：

```text
length ≈ 1
finite
expected count
```

---

# 8. MH2O -> MCLQ — 客户端必须执行

这是客户端 ADT retroport 的核心强制步骤。

真实 Turtle terrain loader没有发现 WotLK MH2O 输入路径，而明确消费 legacy MCLQ。

因此：

```text
WotLK MH2O
    -> parse liquid instances
    -> normalize masks/heights/type
    -> flatten target-inexpressible layers according to policy
    -> generate legacy MCLQ per MCNK
    -> update MCNK liquid flags
    -> rebuild ofsMCLQ / sizeMCLQ
    -> target ADT no longer depends on MH2O
```

不能：

```text
delete MH2O and emit empty MCLQ
```

---

## 9. Normalized liquid

ADT 与 WMO 共用液体语义层：

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

```cpp
struct NormalizedLiquidLayer
{
    uint16_t sourceLiquidType;
    LiquidCategory category;

    uint8_t offsetX;
    uint8_t offsetY;
    uint8_t width;
    uint8_t height;

    std::array<bool, 64> visible;
    std::array<float, 81> heights;

    bool deep;
    bool fishable;
};
```

输入语义来自：

```text
MH2O
LiquidType.dbc
source map/group context
```

输出为 target MCLQ / WMO legacy liquid category。

---

## 10. 多层液体 downgrade policy

MH2O 的表达能力高于 Vanilla MCLQ。

Normalized model必须保留所有源 liquid layers，最后 writer 再决定 legacy flattening。

若一个 target MCNK 无法无损表达：

1. 选择 gameplay-visible principal layer。
2. 保留其有效 tile mask与高度。
3. 输出 `lossy-liquid-flatten` diagnostic。
4. 不允许静默丢层。

最终策略必须用实际 Northrend 多层样本回归。

---

# 11. MCLY / MCAL — 规则已冻结

3.3.5a Reader必须支持：

```text
legacy 4-bit packed  : 2048 bytes/layer
big-alpha raw 8-bit : 4096 bytes/layer
big-alpha RLE       : variable -> 4096 decoded bytes
```

输入提示：

```text
WDT MPHD 0x4          big-alpha map mode
MCLY 0x100            alpha present
MCLY 0x200            RLE compressed
actual layer span     corruption/flag fallback evidence
```

Normalized alpha：

```cpp
std::array<uint8_t, 4096>
```

目标统一写：

```text
packed 4-bit
64 x 64 logical samples
2048 bytes/layer
low nibble first
```

8→4 默认最近量化：

```cpp
q = (alpha + 8) / 17;
```

最大理论重建误差 ≤ 8。

目标：

```text
MCLY 0x200 clear
rebuild ofsAlpha
rebuild MCAL payload
```

完整细节见 `ADT_MCAL_BINARY_ANALYSIS.md`。

---

## 12. MCNK bit15 / alpha edge semantics

Turtle 真客户端确认：

```text
MCNK flags bit15 = 1
 -> full 64x64 alpha is meaningful
 -> no legacy last-row/last-column fix

bit15 = 0
 -> decode 63x63
 -> duplicate last row/column
```

Converter已经拥有完整 normalized 64x64 数据，因此目标第一版：

```cpp
targetMcnk.flags |= (1u << 15);
```

前提：MCAL 与 MCSH 的最后行/列都已 materialize/normalize。

---

# 13. MCSH

MCSH 标准 legacy shadow mask：

```text
64 x 64 bits
= 512 bytes
```

因 target bit15 被设置为 full-edge，Writer 不能依赖客户端复制 shadow 最后一行/列。

因此：

```text
source shadow
 -> normalize full 64x64
 -> materialize legacy edge semantics if needed
 -> write 512-byte target MCSH
```

---

# 14. Holes — 不存在 32→16 空间降级

此前的“WotLK uint32 holes → Vanilla uint16 holes”表述已被真实客户端推翻。

3.3.5a build12340 真客户端函数：

```text
0x7C3B60
```

明确读取：

```asm
MOVZX EDI, WORD PTR [MCNK+0x3C]
```

并用16-entry mask table：

```text
0x0001 ... 0x8000
```

将 8×8 terrain quads按：

```cpp
bit = (y >> 1) * 4 + (x >> 1);
```

映射为4×4 coarse holes。

所以：

```cpp
dst.holes = src.holesLowRes;
```

即可保持 hole topology。

`+0x3E` 是独立 u16，不属于 hole mask 高位；第一版 canonical target可写0并对非零源值报警。

详见 `ADT_HOLES_BINARY_ANALYSIS.md`。

---

# 15. Resource name tables

M2：

```text
MMDX = zero-terminated path blob
MMID = offsets into MMDX
```

WMO：

```text
MWMO = zero-terminated path blob
MWID = offsets into MWMO
```

Writer 从 normalized vector重建整个 string block与offset table，以支持：

- path normalization
- extension normalization
- converted output path remap
- deduplication
- dependency graph consistency

禁止复制 source MMID/MWID offsets。

---

# 16. MDDF / MODF

已知物理基线：

```text
MDDF = 36 bytes
MODF = 64 bytes
```

布局在 3.3.5 extractor 与旧目标体系间高度连续。

第一版原则：

- 保留 world placement position/rotation/scale。
- name/reference index随重建后的 MMDX/MWMO 表更新。
- unique ID尽量保留。
- flags逐语义检查，不把源扩展bit原样复制。
- MODF doodad set / name set保留并验证目标 WMO存在对应集合。

---

# 17. MCRF

MCRF 引用的是 tile 内 placement index，不是文件路径。

由于 MDDF/MODF 可能重排/去重，Writer必须建立：

```text
source MDDF index -> target MDDF index
source MODF index -> target MODF index
```

再重建每个 MCNK 的 doodad/WMO refs。

禁止在 placement table发生重排后原样复制 MCRF。

---

# 18. MCIN / offsets — 两遍写出

目标 Writer采用 two-pass/backpatch。

### Pass 1

构造每个 MCNK：

```text
header placeholder
MCVT
MCNR
MCLY
MCRF
MCSH
MCAL
MCSE
MCLQ
```

记录实际布局。

### Pass 2

回填：

```text
MCNK internal offsets/sizes
MCIN[256] offsets/sizes
MHDR top-level offsets
chunk sizes
```

所有 offsets必须落在最终文件范围内。

---

# 19. WDT — 普通 tile map 基本兼容，重新生成最稳

TC 3.3.5 与 Tortoise source一致：

```text
MPHD = 8 dwords
MAIN = 64 x 64 x 8 bytes
```

普通 map target Writer从实际输出 tile set重建 MAIN。

Turtle/Vanilla runtime真正关键的 MPHD bit目前确认是：

```text
bit0 = WMO-only/global-WMO map
```

目标 MCAL已写成 legacy packed4，因此 canonical target：

```text
clear source big-alpha MPHD 0x4
clear unknown WotLK-only flags
```

普通 terrain map：

```text
MPHD flags = 0
```

除非有明确 target flag需求。

详见 `WDT_WDL_BINARY_ANALYSIS.md`。

---

# 20. WDL — 从 NormalizedADT 重建

不要把 WDL 当必须 byte-retroport 的源文件。

目标低分辨率 tile核心：

```text
17 x 17 outer heights = 289
16 x 16 inner heights = 256
--------------------------------
545 x int16
```

MAOF：

```text
64 x 64 absolute offsets
```

推荐：

```text
NormalizedADT terrain
 -> WdlGenerator
 -> target WDL
```

这样绕开 3.3.5a WDL额外 root/global-object数据差异。

精确 float→int16 WDL quantization仍需用已知 good fixture验证后冻结。

---

# 21. Dependency graph

转换任务单位应是 Map Asset Graph：

```text
Map/WDT
  |- ADT tiles
  |   |- MTEX -> BLP
  |   |- MMDX/MMID -> M2 -> SKIN/ANIM/BLP
  |   |- MWMO/MWID -> WMO -> doodad M2 -> BLP
  |   |- MDDF
  |   `- MODF
  `- WDL target regeneration
```

依赖先扫描，再做路径规划，最后写目标文件。

---

# 22. Client validator

每个目标 ADT至少检查：

### Top-level

```text
MVER/MHDR/MCIN存在
256 MCNK
all chunk boundaries in file
```

### tables

```text
MMID inside MMDX
MWID inside MWMO
all strings terminated
```

### MCNK

```text
indices x/y valid
145 heights
normal count valid
layer count 1..4
MCAL target maps each 2048B
MCLY no compression bit in target
MCSH exactly 512B when present
holes is u16 coarse mask
MCLQ offsets/sizes valid when liquid exists
all refs valid
```

Writer完成后必须重新用 target parser打开自己的输出。

---

# 23. Server/extractor validator

Converted client asset graph必须通过：

```text
Tortoise map extractor
Tortoise vmap extractor
Tortoise vmap assembler
Tortoise mmap generator
```

注意服务器 `.map` 推荐直接从 source MH2O reader生成，不以 target MCLQ作为唯一数据源。

---

# 24. Runtime test matrix

第一批 fixtures：

1. 单纹理、无液体、无 object 平地。
2. 4-layer + alpha terrain。
3. MCSH 边缘明显 terrain。
4. 单 hole bit / 多 hole bit terrain。
5. 小湖。
6. ocean coastline。
7. magma/slime。
8. partial MH2O coverage。
9. M2 placements。
10. WMO placements + WMO doodads。
11. 多 tile seam。

检查：

```text
visual terrain geometry
texture alpha
shadow edges
holes
liquid render/swim/type
M2/WMO transforms
collision / LOS
VMAP/MMAP
```

---

# 25. 第一版实现优先级 — 当前版本

已经研究冻结：

```text
[done] Turtle legacy ADT/MCLQ target loader
[done] MCAL source modes + target packed4
[done] MCNK bit15 alpha edge semantics
[done] holes exact 16-bit spatial mapping
[done] ordinary WDT target structure/policy
[done] WDL regeneration architecture
```

接下来实际编码顺序：

```text
1. WotlkAdtReader
2. NormalizedADT
3. MH2O reader
4. LiquidTypeMapper
5. Mh2oToMclq
6. MCAL decoder + Vanilla4 encoder
7. MCSH normalizer
8. VanillaMcnkWriter + offset rebuilder
9. MMDX/MMID + MWMO/MWID rebuild
10. MDDF/MODF/MCRF remap
11. VanillaAdtWriter
12. WdtWriter
13. WdlGenerator
14. Validators + fixtures
```

---

# 26. Source references

Primary implementation/research references：

- Penqle/tortoise-wow
- TrinityCore `3.3.5`
- AzerothCore WotLK
- wowdev/noggit3
- wowdev/pywowlib
- samwhosung/wow-1121-client-internals
- user-provided WoW 3.3.5a build12340 `Wow.exe`
- user-provided Turtle WoW 1.18.1 build7272 `WoW.exe`

Community format documentation remains secondary; real target/source client behavior is final authority.
