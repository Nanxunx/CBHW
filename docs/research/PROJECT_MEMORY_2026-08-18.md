# 模型移植项目长期记忆 — 2026-08-18

本文件用于保存当前“模型移植 / Turtle335Converter”可直接继承的结论。后续对话优先以本文件和其指向的专题研究文档为准。

## 项目目标

将 WoW 3.3.5a build 12340 客户端资源可靠 retroport 到 Vanilla 1.12.x / Turtle WoW 1.18.1 build 7272 可加载格式，并让 Penqle/tortoise-wow 的 maps/vmaps/mmaps 工具链正确消费。

统一架构：

```text
3.3.5 Reader
 -> Normalized semantic model
 -> semantic downgrade
 -> Vanilla/Turtle Writer
 -> strict validator
 -> runtime/server regression
```

禁止：

```text
只改版本号
raw memcpy 不同世代 header
删除 chunk 后沿用旧 offset
把未验证推断写成格式事实
```

证据等级：`客户端二进制确认 / 源码确认 / 实现确认 / 强推断 / 未验证`。

---

# 1. 目标客户端性质

Turtle WoW 1.18.1 build 7272 的已上传 `WoW.exe` 静态分析确认：

- PE32 i386。
- VERSIONINFO/历史字符串仍保留 Vanilla 1.12.1 build 5875 血统。
- 同时存在 Turtle `7272 / 1.18.1` 标识和 MPQ patch 路径。

结论：目标是**高度修改的 Vanilla 1.12.1-derived 32-bit MPQ client**，不是 WotLK 格式客户端。

---

# 2. 客户端/服务器液体路线必须分开

## 客户端 ADT

```text
3.3.5 MH2O
 -> normalized liquid layers
 -> legacy multi-record MCLQ
 -> Turtle client ADT
```

## 服务器 `.map`

```text
original 3.3.5 MH2O
 -> Trinity-derived active MH2O reader semantics
 -> Tortoise z1.4 .map writer
```

不要：

```text
335 MH2O -> client MCLQ -> Tortoise old-MCLQ extractor -> server map
```

原因：Penqle/tortoise-wow 的 MH2O 结构存在，但实际 map extraction 的 MH2O block 被注释；old-MCLQ server extractor又只看第一份 legacy record。

---

# 3. MCLQ multi-record — 已由 Turtle 真客户端确认

专题：`docs/research/NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md`

Turtle 真客户端 `0x6AF760` 已确认 MCNK liquid category bits：

```text
0x04 River/Water
0x08 Ocean
0x10 Magma
0x20 Slime
```

每个存在类别消费一份独立 **804-byte** record，顺序固定：

```text
River -> Ocean -> Magma -> Slime
```

Noggit old-MCLQ writer 与其一致：

```text
MCNK.sizeMCLQ = 8 + 804*N
MCLQ inner size field = 0
```

目标 cell code：

```text
Ocean = 0x01
Slime = 0x03
Water/River = 0x04
Magma = 0x06
Hidden = 0x0F
bit6 = fishable
bit7 = fatigue/deep
```

旧的“所有液体类别合并进一个 9x9 MCLQ grid”方案已废弃。

当前代码已经实现 category-grouped multi-record，并测试：

- Water+Ocean = 1616B。
- 四类 = 3224B。
- cross-category overlap 可保留。
- same-category overlap/height conflict 会诊断。

---

# 4. ADT root contract — 已实现

专题：`docs/research/ADT_FULL_WRITER_TERRAIN_2026-08-18.md`

Penqle/tortoise-wow + Noggit 交叉确认：

```text
MVER:  REVM, payload 4, version 18, full 12B
MHDR:  RDHM, payload 64, full 72B
MCIN:  NICM, payload 4096
MCIN absolute file start = byte 84
```

MHDR offsets 相对：

```text
MHDR payload start = file byte 20
```

MCIN entry：

```text
uint32 absolute MCNK file offset
uint32 MCNK full byte size
uint32 flags
uint32 asyncId
```

## 重要修正：MCIN 顺序

早期错误：

```text
slot = ix*16 + iy
```

最终正确：

```text
slot = iy*16 + ix
```

来源：Noggit `py*16+px` 实际 writer + Tortoise `getMCNK(i,j)` 使用语义。

测试锁死：

```text
slot0   = (0,0)
slot1   = (1,0)
slot16  = (0,1)
slot255 = (15,15)
```

---

# 5. Full ADT Writer — 已完成第一阶段

当前仓库已经新增：

```text
include/turtle335/adt/PlacementWriter.h
src/adt/PlacementWriter.cpp

include/turtle335/adt/TerrainWriter.h
src/adt/TerrainWriter.cpp

include/turtle335/adt/AdtWriter.h
src/adt/AdtWriter.cpp
```

以及 Fixture/集成测试。

`AdtWriter` 当前可以从结构化输入生成：

```text
MVER
MHDR
MCIN
MTEX
MMDX/MMID
MWMO/MWID
MDDF
MODF
256 * MCNK
```

并统一 backpatch MHDR + MCIN。

Validator 会检查：

- MVER/MHDR/MCIN root。
- MHDR 非零 pointer 对应 FourCC。
- target MH2O offset 必须为 0。
- 256 MCIN ranges。
- MCIN size 与 MCNK declared size 一致。
- MCIN y-major slot 与 MCNK ix/iy 一致。

---

# 6. Placement Writer

WoW-Crucible placement 审计：`docs/research/WOW_CRUCIBLE_PLACEMENT_AUDIT_2026-08-18.md`

MDDF = 36B：

```text
+00 NameId
+04 UniqueId
+08 Position vec3
+20 Rotation vec3
+32 Scale uint16
+34 Flags uint16
```

MODF = 64B：

```text
+00 NameId
+04 UniqueId
+08 Position vec3
+20 Rotation vec3
+32 MinimumExtent vec3
+44 MaximumExtent vec3
+56 Flags uint16
+58 DoodadSet uint16
+60 NameSet uint16
+62 Scale uint16
```

Path catalog：

```text
assetPath -> MMDX/MWMO string
          -> MMID/MWID string-offset entry
          -> MDDF/MODF NameId indexes MMID/MWID
```

当前 Writer：

- `/ -> \`。
- case-insensitive path dedup。
- preserve supplied source UID。
- UID 0 reject。
- duplicate/cross-family UID collision reject。
- MCRF validates index ranges。
- MCRF order = M2 refs then WMO refs。

完整整图转换后续仍要加 `MapPlacementUidRegistry` 做 map-wide collision repair。

---

# 7. Terrain Writer — 已实现

## MCVT

Normalized 输入保存 145 个绝对高度。

Target：

```text
MCNK.ypos = absolute vertex0 height
MCVT[i] = absoluteHeight[i] - absoluteHeight[0]
```

payload = `145*4 = 580B`。

## MCNR

payload = `145*3 = 435B`。

normal 归一化后按 Noggit disk order：

```text
byte0 = x*127
byte1 = z*127
byte2 = y*127
```

### MCNR 13B tail

MCNR declared payload 仍是 435B，但 chunk 后面另有：

```text
13 raw bytes
```

它们不属于 MCNR declared payload，却属于 MCNK physical layout。

因此后续 MCLY 必须跨过这 13B。

## MCLY

16B/layer：

```text
textureId
flags
ofsAlpha
effectId
```

Writer owns：

```text
base: clear USE_ALPHA 0x100, clear COMPRESSED 0x200
nonbase: set USE_ALPHA, clear COMPRESSED
```

old-alpha offsets 相对 MCAL payload start：

```text
0, 2048, 4096
```

## MCAL

不能把 WotLK/big-alpha 每层独立直接 8bit->4bit。

先做 Noggit `alphas_to_old_alpha` 等价转换：

```text
remaining = 255
从 top layer 向下：
sequential = round(currentContribution * 255 / remaining)
remaining -= currentContribution
```

然后再 pack 成 4-bit old alpha。

每 non-base layer：

```text
2048B
```

1-layer 地形仍写空 `LACM` header，full chunk = 8B。

---

# 8. MCNK canonical physical corrections

当前 `McnkWriter` 已修正并统一管理：

```text
bit0     has MCSH
bit2..5  liquid category bits
bit6     has MCCV
bit15    do_not_fix_alpha_map
```

canonical old-MCLQ path：

```text
bit15 = 0
```

早期“MCAL/MCSH 存在就设置 bit15”是错误结论，已修复。

MCSH：

```text
payload = 512B
full raw chunk = 520B
MCNK.sizeMCSH = 512
```

不是 520。

## Canonical empty structural chunks

Noggit old-MCLQ save path 的目标形态已下沉进 `McnkWriter`：

无 placement ref：

```text
FRCM size=0
```

无 sound：

```text
ESCM size=0
nSndEmitters=0
```

无 liquid record：

```text
QLCM inner size=0
MCNK.sizeMCLQ=8
liquid category bits=0
```

`McnkWriter` 现在从 MCSE payload / `0x1C` 自动推导 `nSndEmitters`。

---

# 9. Fixture A — 已生成真实 ADT artifact

Generator：

```text
tools/generate_fixture_a.cpp
CMake target: turtle335_generate_fixture_a
```

Profile：

```text
32,32 coordinate basis
256 MCNK
flat height 50
up normals
1 base texture layer
no M2/WMO placement
no liquid records
no sounds
```

测试 texture：

```text
Tileset\Elwynn\ElwynnGrassBase.blp
```

该路径有老客户端/老地图工具资料支持；最终仍需目标 Turtle archive presence/runtime确认。

CI-generated file：

```text
FixtureA_32_32.adt
size = 320647 bytes
SHA-256 = cc6fa91c07d150ace092c13a740c3957c24d45e6e7f8731222562a2683ed02d4
```

GitHub Actions artifact：

```text
name = Turtle335Converter-FixtureA
artifact id = 9297587206
```

独立 Python binary inspection（不使用 C++ validator）再次确认：

```text
REVM/18
RDHM/64
NICM/4096
MCIN y-major
first MCNK size 1236
MCVT 580
MCNR 435 + external 13B
MCLY 16
FRCM 0
LACM 0 / sizeMCAL 8
ESCM 0
QLCM 0 / sizeMCLQ 8
```

---

# 10. CI 状态

Workflow：

```text
.github/workflows/core-tests.yml
```

使用 Debug 是刻意的：当前测试大量使用 `assert()`，Release/NDEBUG 会删除断言。

最终 canonical Fixture A run：

```text
run id = 32059491564
head = f1ebff6397028d24831a574cdc588962a3c0ad0a
```

结果：

```text
Ubuntu: configure PASS, build PASS, 5/5 CTest PASS,
        Fixture A generation PASS, SHA PASS, artifact upload PASS
Windows: configure PASS, build PASS, 5/5 CTest PASS
```

早期 Windows segfault 已确认是测试自己的默认 1MiB stack overflow：`AdtWriterInput` 太大且测试在栈上复制多份。现已改 heap allocation。

---

# 11. ADT 当前 confidence boundary

## 源码/实现已确认

```text
root layout
MHDR backpatch
MCIN absolute offsets + y-major ordering
MMDX/MMID/MWMO/MWID
MDDF/MODF
MCRF
MCVT
MCNR + 13-byte tail
MCLY
old-alpha MCAL semantic conversion
canonical MCNK flags
canonical empty MCRF/MCSE/QLCM
cross-platform build/tests
deterministic Fixture A generation
```

## 仍未验证

```text
Noggit GUI 对 FixtureA 无修复打开/保存/重开
Turtle 1.18.1 WoW.exe 实机渲染 FixtureA
Tortoise server extractor 对这个 synthetic client tile 的完整行为
```

未经过实机前，禁止把“结构测试通过”描述成“客户端已经兼容”。

---

# 12. M2 关键保留结论

- WotLK 3.3.5 与 Vanilla/Turtle M2 Header/子结构真实不同，不能只改 version。
- server VMAP 对 M2 主要需要 bounding geometry；client retroport 需要完整转换。
- WoW-Crucible `M2PlacementBoundsService` 明确按 MD20 **v264**、vertex stride 48、count `0x3C`、offset `0x40` 读取 WotLK M2。
- WoW-Crucible `StaticM2DownportService` 是 modern v274 -> WotLK v264，不是 335->Vanilla。
- M2Workshop 是高价值 repair/validator reference，但 GPL-3.0；优先独立实现。
- `jM2converter` CLI 很薄，后续必须追真正 jM2lib/M2Lib conversion implementation。

---

# 13. WMO 关键保留结论

WotLK Trinity MOPY 与 Tortoise legacy MOPY bit semantics 不同，必须 semantic remap，禁止 raw bit copy。

WMO MVER=17 不能单独区分 Vanilla/WotLK。

转换器后续要处理：

```text
MOPY semantic remap
MOGP flag downgrade
second MOCV/MOTV downgrade/bake
MOMT/material differences
MLIQ
MODS/MODD
collision semantics
```

WMO MODF bounds rotate/scale 时应从 exact WMO root `MOHD` local bounds 重算；仅 translation 可按 delta 平移现有 extents。

---

# 14. Server map/vmap/mmap 保留架构

```text
server .map:
original 335 ADT
 -> Trinity-style 335 reader
 -> Tortoise z1.4 writer

server vmap:
converted ADT/M2/WMO
 -> Tortoise vmap_extractor
 -> VMAP005
 -> vmap_assembler

server mmap:
Tortoise target maps + vmaps
 -> Tortoise mmap
```

不要让 Tortoise mmap 直接解析 raw 335 ADT。

---

# 15. 当前下一步优先级 — 已更新

Full ADT Writer 的“结构实现”不再是当前瓶颈。

## P0 — Runtime Fixture A

```text
1. 使用 CI artifact FixtureA_32_32.adt
2. 放进一次性/可回滚的现有 WDT 已启用 tile 位置
3. Noggit old-MCLQ mode 打开
4. save -> reopen
5. 对 Noggit 输出做结构 diff
6. Turtle WoW 1.18.1 真客户端进入对应 tile
```

如果失败，先只修 Fixture A，不增加 placement/liquid/M2 变量。

## P0 — Fixture B

Fixture A runtime PASS 后：

```text
2 texture layers
real old-alpha MCAL
```

验证纹理 blending。

## P0 — Fixture C

Fixture B PASS 后：

```text
MMDX/MMID + MDDF
MWMO/MWID + MODF
MCRF
```

验证 placement。

## P0 — Fixture D

Fixture C PASS 后：

```text
River/Ocean/Magma/Slime multi-record MCLQ
```

验证真实客户端液体。

## 下一大阶段 — M2

只有客户端 ADT fixture 基线稳定后，进入：

```text
M2 v264 source -> Vanilla/Turtle target
```

优先审计：

```text
actual jM2lib/M2Lib implementation
Koward/M2Lib lineage
M2Workshop
Turtle WoW.exe loader
warcraft-rs
pywowlib
```

## P1

```text
MapPlacementUidRegistry
LegacyAssetPathResolver
ConversionManifest
WMO semantic downgrade
DBC schema cross-check
runtime regression corpus
```

---

# 16. 文档优先级 / 旧结论淘汰

当前优先参考：

```text
PROJECT_MEMORY_2026-08-18.md
ADT_FULL_WRITER_TERRAIN_2026-08-18.md
NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md
WOW_CRUCIBLE_PLACEMENT_AUDIT_2026-08-18.md
```

旧 `ADT_MH2O_MCLQ_CONVERSION.md` 中下列早期结论已经被后续证据取代，不得再用于实现：

```text
“一个 MCNK 只能有一份 MCLQ record”
“不同 category 必须共享单一 9x9 grid”
“Slime cell code = 0x06”
“fishable 没有 legacy bit”
```

最新实现必须以 multi-record cross-check 和本长期记忆为准。
