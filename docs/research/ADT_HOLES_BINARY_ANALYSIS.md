# ADT MCNK Holes Binary Analysis — 3.3.5a to Turtle/Vanilla

状态：**3.3.5a real-client binary confirmed + cross-source confirmed**

目标：解决此前“TrinityCore 3.3.5 `uint32 holes` 是否代表 32 个 hole bits、是否必须做 32→16 空间降级”的歧义。

---

## 1. Final correction

> **WoW 3.3.5a build 12340 的真实地形 hole mask 仍然是 MCNK `+0x3C` 的 16-bit、4×4 coarse mask。**

`+0x3E` 是另一个独立的 16-bit 字段，不应被解释成 hole mask 的高16位。

因此：

```text
3.3.5a client hole semantics
=
Vanilla/Turtle 16-bit coarse hole semantics
```

对 build 12340 的普通 MCNK hole mask，**不存在 32-hole-bit → 16-hole-bit 的空间压缩问题**。

---

## 2. Real 3.3.5a client proof

客户端：

```text
WoW 3.3.5a build 12340
Wow(1).exe
```

terrain triangle/index builder：

```text
0x7C3B60
```

关键指令：

```asm
MOV EDI,[... MCNK pointer ...]
MOVZX EDI,WORD PTR [EDI+0x3C]
```

即：

```text
holes = uint16(MCNK + 0x3C)
```

不是：

```text
uint32(MCNK + 0x3C)
```

证据等级：**真实客户端二进制确认**。

---

## 3. Exact spatial mapping

同一函数遍历 terrain chunk 的：

```text
8 × 8 quads
```

对 quad coordinates `(x,y)`，客户端逻辑等效于：

```cpp
int coarseX = x >> 1;
int coarseY = y >> 1;
int holeIndex = coarseY * 4 + coarseX;
```

然后从静态 mask table 取 bit：

```text
0xA3FAF0
```

该表从真实 `Wow.exe` 数据段读出：

```text
index  0 -> 0x0001
index  1 -> 0x0002
index  2 -> 0x0004
index  3 -> 0x0008
index  4 -> 0x0010
index  5 -> 0x0020
index  6 -> 0x0040
index  7 -> 0x0080
index  8 -> 0x0100
index  9 -> 0x0200
index 10 -> 0x0400
index 11 -> 0x0800
index 12 -> 0x1000
index 13 -> 0x2000
index 14 -> 0x4000
index 15 -> 0x8000
```

因此：

> **一个 hole bit 覆盖 2×2 terrain quads；16 bits 构成 4×4 coarse mask。**

---

## 4. Triangle generation behavior

`0x7C3B60` 对每个 8×8 quad 测试：

```cpp
if (holes & mask[(y >> 1) * 4 + (x >> 1)])
{
    // skip this quad's terrain triangles
}
else
{
    // emit normal terrain triangles
}
```

因此洞的视觉几何与碰撞/terrain topology 是由相同 coarse grouping 驱动。

---

## 5. pywowlib cross-check

`wowdev/pywowlib` 的 MCNK header 明确拆成：

```python
self.holes_low_res = uint16.read(f)
self.unknown_but_used = uint16.read(f)
```

也就是：

```text
+0x3C u16 holes_low_res
+0x3E u16 other field
```

它写出时也按相同两个 `uint16` 独立序列化。

这与真实 3.3.5a client loader 完全一致。

---

## 6. Why TrinityCore looks like uint32 holes

TrinityCore 3.3.5 `map_extractor/adt.h` 使用：

```cpp
uint32 holes;
```

占据同样的4字节空间。

但其 `.map` 导出逻辑把该值直接赋给：

```cpp
uint16 holes[16][16];
```

因此 TC 自己在服务器导出时也只保留低16位。

结合真实客户端证据，正确解释是：

> Trinity extractor 的 `uint32 holes` 是历史/简化布局声明，不应被解读为“build 12340 客户端拥有32个 coarse hole bits”。

---

## 7. HIGH_RES_HOLES flag correction

跨版本解析库中可见：

```text
MCNK flags bit16 = HIGH_RES_HOLES
```

但这不意味着：

```text
MCNK + 0x3C upper 16 bits = high-resolution holes
```

当前 build 12340 真实 terrain index builder明确只读取：

```text
WORD [MCNK+0x3C]
```

并使用标准16-entry coarse mask table。

因此第一版 3.3.5a→Turtle converter：

- 不把 `+0x3E` 合并到 hole mask。
- 不尝试把所谓32个 bits 降成16个。
- 对 source MCNK flags bit16 单独记录 diagnostic；不要用它改变 `+0x3C` 的解释，除非未来发现 build12340 的另一条明确 high-resolution hole data path。

---

## 8. Target Writer rule

目标 Turtle/Vanilla header：

```cpp
struct VanillaMcnkHeaderFragment
{
    ...
    uint32_t areaId;
    uint32_t nMapObjRefs;
    uint16_t holes;
    uint16_t padOrLegacyField;
    ...
};
```

第一版写出：

```cpp
dst.holes = src.holesLowRes;
```

对第二个16-bit字段：

```text
推荐默认策略：preserve source raw value in normalized metadata,
目标 Writer canonical mode 可写0；compat mode可原样保留。
```

由于 Turtle terrain hole consumer不需要该值，不能让它参与 hole conversion。

建议默认：

```cpp
dst.padOrLegacyField = 0;
```

同时 Validator 若 source 值非0则记录：

```text
warning: MCNK +0x3E non-zero legacy/unknown field was canonicalized
```

在获得该字段 build12340 具体客户端消费者前，不静默赋予地形语义。

---

## 9. Normalized representation

不要再定义：

```cpp
uint32_t sourceHoles;
```

改成：

```cpp
struct NormalizedHoleData
{
    uint16_t coarseMask;

    // raw +0x3E field retained only for diagnostics/round-trip research
    uint16_t sourceUnknown3E;

    bool sourceHighResFlag;
};
```

Helper：

```cpp
bool IsQuadHole(uint16_t holes, int x, int y)
{
    int bit = (y >> 1) * 4 + (x >> 1);
    return (holes & (uint16_t(1u) << bit)) != 0;
}
```

Bounds：

```text
0 <= x < 8
0 <= y < 8
```

---

## 10. Regression tests

### Bit mapping unit test

For each bit `0..15`：

```text
set exactly one bit
expect exactly its 2×2 quad region to be skipped
expect all other quads present
```

### Binary-layout test

Construct MCNK header：

```text
+0x3C = 0x8001
+0x3E = 0xA55A
```

Parser must produce：

```text
holesLowRes     = 0x8001
sourceUnknown3E = 0xA55A
```

not：

```text
holes = 0xA55A8001
```

### Target test

After conversion：

```text
Turtle terrain index count
=
expected index count after removing hole quads
```

and Tortoise map extractor must report the same coarse hole geometry.

---

## 11. Confidence table

| Finding | Confidence |
|---|---|
| 3.3.5a reads holes as u16 at +0x3C | **Real client binary confirmed** |
| hole mask is 4×4 | **Real client binary confirmed** |
| each bit covers 2×2 of 8×8 quads | **Real client binary confirmed** |
| mask order bit0..bit15 sequential | **Real client binary confirmed** |
| +0x3E is separate u16 | Binary layout + pywowlib source confirmed |
| TC uint32 is not 32 spatial hole bits | Strong conclusion from client + TC behavior |
| target holes can copy source low16 directly | **High confidence conversion rule** |
| target +0x3E may be canonicalized to zero | Conservative target rule; exact source meaning pending |

---

## 12. Consequence for project plan

Remove the old blocker：

```text
"design 32-bit WotLK holes -> 16-bit Vanilla spatial downsample"
```

from the critical path.

The real task is now simply：

```text
read +0x3C as u16
preserve semantic coarse mask
write target +0x3C u16
```

This substantially reduces ADT terrain geometry conversion risk.
