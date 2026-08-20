# ADT Full Writer + Terrain Writer — 2026-08-18

状态：**实现完成到 Fixture A；双平台 Debug CI 通过；尚未宣称 Turtle 客户端实机加载通过。**

证据等级：

- `源码确认`：Penqle/tortoise-wow、wowdev/noggit3、WoW-Crucible 的实际源码行为。
- `实现确认`：Turtle335Converter 单元/集成测试与独立二进制解析。
- `未验证`：Noggit GUI 重开与 Turtle WoW 1.18.1 真客户端运行时加载。

---

## 1. 本轮完成范围

新增/完善：

```text
PlacementWriter
TerrainWriter
AdtWriter
McnkWriter canonicalization
Fixture A generator
Windows + Ubuntu CI
```

现在转换器已经可以从结构化输入生成一张完整的 legacy/Turtle-target ADT，而不再只有 MCNK/MCLQ 子模块。

---

## 2. ADT root physical contract

### 2.1 MVER

目标：

```text
raw FourCC = REVM
payload size = 4
version = 18
full chunk = 12 bytes
```

`源码确认`：Penqle/tortoise-wow `FILE_FORMAT_VERSION = 18`。

### 2.2 MHDR

目标：

```text
raw FourCC = RDHM
payload size = 64
full chunk = 72 bytes
```

MHDR 内所有有效 offs 字段都相对：

```text
MHDR payload start
```

即文件 byte 20。

### 2.3 MCIN fixed root position

Canonical root：

```text
MVER 12B
MHDR 72B
MCIN starts byte 84
```

`源码确认`：Tortoise legacy loader 的 `this + offsMCNK - 84` 明确依赖这个根布局。

MCIN：

```text
raw FourCC = NICM
payload = 256 * 16 = 4096 bytes
```

每项：

```text
uint32 absolute MCNK file offset
uint32 MCNK full byte size
uint32 flags
uint32 asyncId
```

---

## 3. MCIN ordering — 已修正的重要 bug

早期 Full Writer 错误按：

```text
slot = ix * 16 + iy
```

写 MCIN。

Noggit 实际保存代码是：

```text
slot = py * 16 + px
```

Tortoise `ConvertADT` 对 `getMCNK(i,j)` 的使用也与第一维=y、第二维=x一致。

因此最终 canonical 规则：

```text
slot = iy * 16 + ix
```

测试已经锁死：

```text
slot 0  = (0,0)
slot 1  = (1,0)
slot 16 = (0,1)
slot255 = (15,15)
```

`源码确认 + 实现确认`。

---

## 4. Placement tables

`PlacementWriter` 实现：

```text
MMDX + MMID
MWMO + MWID
MDDF
MODF
MCRF
```

### MDDF

36 bytes/record：

```text
+00 uint32 NameId
+04 uint32 UniqueId
+08 vec3 Position
+20 vec3 Rotation
+32 uint16 Scale
+34 uint16 Flags
```

### MODF

64 bytes/record：

```text
+00 uint32 NameId
+04 uint32 UniqueId
+08 vec3 Position
+20 vec3 Rotation
+32 vec3 MinimumExtent
+44 vec3 MaximumExtent
+56 uint16 Flags
+58 uint16 DoodadSet
+60 uint16 NameSet
+62 uint16 Scale
```

### Path catalogs

Writer：

- `/` -> `\`
- case-insensitive dedup
- stores path in MMDX/MWMO
- stores string offset in MMID/MWID
- MDDF/MODF `NameId` indexes MMID/MWID

### UniqueID

当前 Full Writer：

- preserves supplied source UID
- rejects UID 0
- rejects duplicate UID
- rejects M2/WMO cross-family UID collision inside one ADT

完整 map-wide UID registry 仍是后续整图 conversion 层职责。

---

## 5. MCRF

MCRF payload顺序：

```text
all M2/MDDF indices
then all WMO/MODF indices
```

Full Writer validates every index against placement table count and derives：

```text
MCNK.nDoodadRefs
MCNK.nMapObjRefs
```

Canonical dry/no-placement MCNK 仍写：

```text
FRCM
size = 0
```

而不是彻底省略 MCRF。

`源码确认`：Noggit save path。

---

## 6. MCVT

输入 normalized terrain 使用 145 个**绝对高度**。

目标写法：

```text
MCNK.ypos = vertex[0].absoluteHeight
MCVT[i]   = vertex[i].absoluteHeight - vertex[0].absoluteHeight
```

MCVT：

```text
145 * float = 580-byte payload
```

`源码确认`：Noggit `MapChunk::save`。

---

## 7. MCNR

目标 MCNR declared payload：

```text
145 * 3 = 435 bytes
```

每个 normalized normal 先归一化，再按 Noggit 顺序写：

```text
byte0 = x * 127
byte1 = z * 127
byte2 = y * 127
```

### 13-byte legacy tail

MCNR chunk header仍声明：

```text
435 bytes
```

但之后另追加：

```text
13 raw bytes
```

这些字节：

- 不属于 MCNR declared payload
- 属于 MCNK physical layout
- 后续 MCLY offset 必须跨过这 13B

因此：

```text
ofsMCLY = ofsMCNR + 8 + 435 + 13
```

`源码确认 + 实现确认`。

---

## 8. MCLY

16 bytes/layer：

```text
uint32 textureId
uint32 flags
uint32 ofsAlpha
uint32 effectId
```

Writer owns alpha-related flags：

```text
base layer:
  clear USE_ALPHA 0x100
  clear COMPRESSED 0x200

non-base:
  set USE_ALPHA 0x100
  clear COMPRESSED 0x200
```

`ofsAlpha` 相对：

```text
MCAL payload start
```

old-alpha 下每个 non-base layer 2048B，因此典型 offsets：

```text
layer1 = 0
layer2 = 2048
layer3 = 4096
```

---

## 9. MCAL — 不能逐层直接 8bit→4bit

Normalized/WotLK big-alpha 被视为“实际 layer contribution”。

Legacy old-alpha 是 sequential coverage。

因此必须先做 Noggit-equivalent `alphas_to_old_alpha`：

```text
remaining = 255
from top layer down:
    sequential = round(currentContribution * 255 / remaining)
    remaining -= currentContribution
```

然后再把 sequential 8-bit alpha 两两压成 4-bit nibbles。

当前 Writer：

- 每像素验证 non-base actual contributions 总和 <= 255
- 做跨层 sequential 转换
- 再调用 old 4-bit encoder
- 每 non-base layer = 2048B

即：

```text
1 layer -> MCAL payload 0, full chunk 8B
2 layer -> 2048 payload
3 layer -> 4096 payload
4 layer -> 6144 payload
```

`源码确认 + 实现确认`。

---

## 10. MCNK managed flags — 已修正的重要 bug

Canonical Writer现在管理：

```text
bit0  = has MCSH
bit2..5 = River/Ocean/Magma/Slime category bits
bit6  = has MCCV
bit15 = do_not_fix_alpha_map
```

old-MCLQ target path：

```text
bit15 MUST be cleared
```

早期 Writer 曾错误地因为 MCAL/MCSH 存在而设置 bit15；已修正并加入测试。

`源码确认`：Noggit old-MCLQ save path。

---

## 11. MCSH size semantic

MCSH：

```text
payload = 0x200 = 512 bytes
full raw chunk = 520 bytes
```

但 MCNK header：

```text
sizeMCSH = 512
```

不是 520。

早期 Writer 曾写 full chunk size；已修正。

---

## 12. Canonical empty MCSE

无 sound emitter 时仍写：

```text
ESCM
size = 0
```

`McnkWriter` 现在从 MCSE payload size / `0x1C` 自动推导 `nSndEmitters`，避免 header/count 不一致。

Fixture A：

```text
nSndEmitters = 0
```

---

## 13. Canonical dry MCLQ

Turtle/Noggit multi-record规则另见：

```text
docs/research/NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md
```

本轮补充 dry-cell physical convention：

```text
QLCM
inner size = 0
no 804-byte records
MCNK.sizeMCLQ = 8
liquid category bits = 0
```

`MclqWriter` 现在空 `LegacyMclqBlock` 返回 8B QLCM header，而不是空 vector。

---

## 14. Fixture A

Generator：

```text
tools/generate_fixture_a.cpp
```

CMake target：

```text
turtle335_generate_fixture_a
```

Fixture profile：

```text
Tile coordinate model: 32,32 basis
256 MCNK
flat terrain height = 50
up normals
1 texture layer
no M2 placements
no WMO placements
no liquid records
no sound emitters
```

Texture path：

```text
Tileset\Elwynn\ElwynnGrassBase.blp
```

该 path 是老客户端/老地图工具资料中长期使用的 Elwynn base terrain texture；最终仍以目标 Turtle client archive presence 为运行时校验。

Generated artifact：

```text
FixtureA_32_32.adt
size = 320647 bytes
SHA-256 = cc6fa91c07d150ace092c13a740c3957c24d45e6e7f8731222562a2683ed02d4
```

Artifact bundle：

```text
Turtle335Converter-FixtureA
GitHub Actions artifact id = 9297587206
```

---

## 15. Independent binary inspection

CI-generated ADT was downloaded and parsed separately from the C++ validator.

Confirmed：

```text
file size = 320647
MVER = REVM, size 4, version 18
MHDR = RDHM, size 64
MCIN = NICM, size 4096
```

MHDR：

```text
offsMCIN resolves byte 84
offsMTEX resolves XETM
MH2O = 0
```

MCIN examples：

```text
slot 0   -> ix0,  iy0
slot 1   -> ix1,  iy0
slot 15  -> ix15, iy0
slot 16  -> ix0,  iy1
slot 255 -> ix15, iy15
```

First MCNK：

```text
full size = 1236
payload   = 1228

MCVT offset 136  -> TVCM payload 580
MCNR offset 724  -> RNCM payload 435
MCLY offset 1180 -> YLCM payload 16
MCRF offset 1204 -> FRCM payload 0
MCAL offset 1212 -> LACM payload 0, sizeMCAL 8
MCSE offset 1220 -> ESCM payload 0
MCLQ offset 1228 -> QLCM inner size 0, sizeMCLQ 8
```

`实现确认`。

---

## 16. CI evidence

Workflow：

```text
.github/workflows/core-tests.yml
```

Debug configuration is intentional because tests use `assert()`; Release/NDEBUG would remove those assertions.

Final canonical Fixture A workflow run：

```text
run id = 32059491564
head commit = f1ebff6397028d24831a574cdc588962a3c0ad0a
```

Results：

```text
Ubuntu: configure PASS / build PASS / 5 CTest PASS / fixture generation PASS / artifact upload PASS
Windows: configure PASS / build PASS / 5 CTest PASS
```

Earlier Windows Stack Overflow in `test_full_adt_writer` was a **test-only stack allocation bug**: `AdtWriterInput` is large because it owns 256 MCNK structures. Tests now allocate these large objects on the heap.

---

## 17. Current confidence boundary

### Confirmed

```text
ADT root writer physical structure
MHDR/MCIN backpatch
MCIN y-major order
placement tables + refs
MCVT
MCNR + 13B tail
MCLY
old-alpha MCAL conversion
canonical managed MCNK flags
empty MCRF/MCSE/QLCM
cross-platform C++ build/test
Fixture A deterministic generation
```

### NOT yet claimed

```text
Noggit GUI opens/saves FixtureA without repair
Turtle 1.18.1 WoW.exe renders FixtureA in world
server extractor accepts this exact synthetic tile as a gameplay map input
```

Those require external application/runtime testing and must remain labelled `未验证` until actually run.

---

## 18. Immediate next step

Do not start M2 yet.

Next validation order：

```text
1. Download FixtureA_32_32.adt
2. Insert/rename into a disposable known map tile slot whose WDT already marks the tile present
3. Open/save/reopen with Noggit old-MCLQ mode
4. Binary diff structural invariants after Noggit save
5. Load same disposable map through Turtle WoW 1.18.1
6. Only after Fixture A runtime PASS -> Fixture B (2-layer MCAL)
7. Then Fixture C (M2 + WMO placements)
8. Then Fixture D (multi-record MCLQ)
9. Then return to M2 v264 -> Vanilla/Turtle target
```

This ordering minimizes the number of moving parts when the first real-client failure occurs.
