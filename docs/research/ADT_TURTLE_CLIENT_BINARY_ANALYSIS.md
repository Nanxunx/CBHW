# Turtle ADT Client Binary Analysis — MH2O vs MCLQ

状态：**real-client binary confirmed**

目标：确定 Turtle WoW 1.18.1 客户端自身是否支持 WotLK `MH2O`，从而决定 3.3.5a ADT retroport 是否必须执行 `MH2O -> MCLQ`。

---

## 1. Result

> **Turtle WoW 1.18.1 的实际 terrain loader 仍是 Vanilla 1.12.1 风格的 MCNK/MCLQ 路径。当前二进制分析未发现 WotLK MH2O 输入路径。客户端 ADT retroport 必须生成旧式 MCLQ。**

这与 Penqle/tortoise-wow 的服务器 extractor 能“认识部分 MH2O 结构”是两个不同问题：

- Client renderer/load path 决定客户端 MPQ 中 ADT 能否显示。
- Server extractor 决定服务器 `.map` / liquid 数据能否从源文件生成。

---

## 2. Turtle ADT root parser

真实 Turtle WoW 1.18.1 `WoW.exe`：

```text
0x6C2010
```

为 ADT full-tile parse 路径。

它从 resident ADT buffer 取得 MHDR，并按 MHDR 的旧 offset table 直接建立：

```text
MCIN
MTEX
MMDX / MMID
MWMO / MWID
MDDF
MODF
```

等指针。

该路径与独立 Vanilla 1.12.1 客户端逆向资料中的 `0x6C2010` 完全一致。

重要特征：

- offset-table driven
- 不靠扫描 FourCC 来发现 WotLK 扩展
- 使用 Vanilla MHDR/MCNK 布局

---

## 3. MCNK fixup

真实客户端函数：

```text
0x6AF970
```

将 MCNK header 内的旧式子块 offsets 修正成 runtime pointers。

可观察到它从 MCNK header 读取多个固定位置的 offsets 并建立：

```text
MCVT
MCNR
MCLY
MCRF
MCAL
MCSH
MCSE
MCLQ
```

对应 runtime pointers。

这是一条典型 Vanilla MCNK fixed-layout path，而不是 WotLK MH2O discovery path。

---

## 4. MCLQ real consumer

真实客户端函数：

```text
0x68D540
```

构建旧式液体 surface。

它按 Vanilla MCLQ 的 9x9 liquid vertex grid 消费数据，并从旧 liquid block 中获取高度/flags。

对应旧格式：

```text
MCLQ
  minHeight / maxHeight
  9 x 9 liquid vertices
  8 x 8 cell flags
  flow data
```

独立 Vanilla 1.12.1 逆向资料也将该函数标识为 MCLQ liquid surface consumer。

---

## 5. Why raw WotLK ADT is not a valid Turtle client target

WotLK 3.3.5a 通常将 terrain liquid 数据放在 tile-level `MH2O` 中，而 Vanilla terrain loader 依赖：

```text
MCNK flags
+
MCNK offsMCLQ
+
legacy MCLQ payload
```

因此：

```text
3.3.5 ADT with MH2O
        ->
raw-copy to Turtle patch
        ->
NOT a supported client conversion strategy
```

即使 Tortoise server-side extractor/header definitions 中存在 `adt_MH2O` 结构，也不能据此推断 Turtle client renderer 支持 MH2O。

---

## 6. Required client conversion

正式客户端 pipeline：

```text
WotLK ADT
  MH2O
    -> parse all liquid instances
    -> normalize LiquidType / masks / heights
    -> flatten to Vanilla-representable liquid layers
    -> generate MCNK MCLQ blocks
    -> update MCNK liquid flags
    -> update offsMCLQ / sizeMCLQ
    -> remove/zero target MH2O reference
    -> rebuild MCNK sizes
    -> rebuild MCIN
    -> rebuild MHDR
  -> Vanilla/Turtle ADT
```

不能只删除 MH2O；必须重建可见液体表面。

---

## 7. Server path remains different

服务器数据生成不需要先把 MH2O 转成 MCLQ。

推荐：

```text
Original 3.3.5 ADT
        ->
Trinity/AzerothCore-style MH2O reader
        ->
Normalized terrain/liquid
        ->
Tortoise target .map writer
```

这样避免：

```text
MH2O -> MCLQ -> parse MCLQ -> server map
```

的额外精度损失和降级信息丢失。

因此项目必须明确分成：

```text
ClientAssetRetroport
ServerDataGeneration
```

两个输出阶段。

---

## 8. Conversion invariant

客户端 Writer 完成后至少验证：

```text
- 256 MCNK present
- MCNK legacy header offsets valid
- MCNK liquid flags agree with emitted MCLQ slots
- offsMCLQ points to an in-bounds MCLQ record
- MCLQ 9x9 heights finite
- MCLQ 8x8 flags valid
- target ADT does not depend on MH2O for visible liquid
```

最终验证：

```text
Turtle WoW 1.18.1 real client loads tile
+
water renders at correct height
+
liquid query/swim state works
+
Tortoise maps/vmaps/mmaps generation succeeds
```

---

## 9. Cross-reference

Independent Vanilla client research:

```text
samwhosung/wow-1121-client-internals
  docs/terrain.md
```

records the same function addresses and confirms Vanilla ADT liquid is the legacy MCLQ route.

The Turtle binary matching these code paths is strong evidence that this subsystem was retained from the Vanilla-derived client rather than replaced with a WotLK terrain loader.
