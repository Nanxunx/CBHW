# Noggit3 × Turtle × Tortoise MCLQ Cross-check — 2026-08-18

状态：**核心结论已升级为客户端二进制确认 + 多源码交叉确认。**

本文件修正此前 `ADT_MH2O_MCLQ_CONVERSION.md` 中“把不同液体类别合并进单一 804-byte MCLQ record”的设计。本文结论优先级更高，尤其**取代旧文档第 16–27 节中跨类别单-grid merge 的假设**。

## 1. 结论摘要

Turtle/Vanilla legacy MCLQ 并不是“每个 MCNK 只能有一个 804-byte 液体 record”。真实目标客户端按照 MCNK 液体类别 flags，最多顺序消费四个独立的 804-byte MCLQ record：

```text
0x04 River/Water
0x08 Ocean
0x10 Magma
0x20 Slime
```

目标 raw layout 应采用：

```text
QLCM
uint32 0                    // canonical inner chunk size used by Noggit old-liquid export
804-byte River record       // only when flag 0x04 exists
804-byte Ocean record       // only when flag 0x08 exists
804-byte Magma record       // only when flag 0x10 exists
804-byte Slime record       // only when flag 0x20 exists
```

并且：

```text
MCNK.sizeMCLQ = 8 + 804 * number_of_present_categories
```

因此不同类别之间不需要争用同一个 9×9 height / payload grid。

---

## 2. Turtle real-client binary confirmation

**证据等级：客户端二进制确认。**

Turtle 1.18.1 `WoW.exe` 的 legacy liquid fixup/runtime path around `0x6AF760`：

1. 从 mask `0x04` 开始；
2. 固定循环四次，每次 mask 左移一位；
3. 对 MCNK flags 中存在的类别位，消费一份 legacy liquid record；
4. 每份 record 精确前进：

```text
8-byte min/max
648-byte 81×8 vertex block
64-byte tile flags
4-byte flow count
80-byte flow storage
-------------------
804 bytes
```

类别遍历顺序因此为：

```text
River -> Ocean -> Magma -> Slime
```

这说明 record 顺序由 MCNK category bits 定义，不是任意 layer order。

---

## 3. Noggit3 old-MCLQ writer confirmation

**证据等级：源码确认。**

Noggit commit `2893aadcaf3e46b88a363b70bb2524607e8ead7c` 实现了 “save liquids as MCLQ”。当前 `MapChunk::save()`：

```text
liquids_size = 8 + displayed_layer_count * sizeof(mclq)
MCNK.sizeLiquid = liquids_size
```

`liquid_chunk::save_mclq()`：

- 写 MCLQ chunk header；
- inner chunk size 写 0；
- 将每个 804-byte `mclq` record 连续写入；
- 根据类别设置 MCNK liquid flags；
- 按 `mclq_flag_ordering()` 排序。

排序函数为：

```text
River = 0
Ocean = 1
Lava  = 2
Slime = 3
```

与 Turtle real-client 的四类消费顺序完全一致。

Noggit issue #71 及修复 commit `8ec2e02d3109dcbcac3030eb9146f61f34fd7748` 还说明：如果保存 MH2O 却残留旧 MCLQ flags，客户端会根据 flags 把随机数据当成 MCLQ 读取。因此 flags 与 record presence 必须严格同步。

---

## 4. Legacy cell byte corrected semantics

**证据等级：Noggit 源码确认；需继续用 Turtle fixture 验证 fishable/fatigue 的视觉/gameplay效果。**

Noggit `mclq_tile`：

```text
bits 0..2 : liquid_type
bit 3     : dont_render
bit 6     : fishable
bit 7     : fatigue
```

`from_mclq()` / `changeLiquidID()` 共同确认的 low-type code：

```text
1 = Ocean
3 = Slime
4 = River/Water
6 = Magma/Lava
7 + dont_render = hidden byte 0x0F
```

因此此前项目代码把 Slime 映射成 `0x06` 是错误的，应修正为：

```text
Slime = 0x03
Magma = 0x06
```

旧文档中“bit7 仅 dark/deep ocean”的解释也需要修订。Noggit 将 bit7 明确作为 fatigue，bit6 作为 fishable，并由 MH2O attributes 回填。最终 target policy 应以 real-client fixture + Noggit 双证据继续验证，但第一版代码至少不应再把 fishable 判定为“无法编码”。

---

## 5. Correct target data model

推荐目标模型：

```cpp
enum class LegacyLiquidSlot : uint8_t
{
    River = 0,
    Ocean = 1,
    Magma = 2,
    Slime = 3
};

struct LegacyMclqRecord
{
    LiquidCategory category;
    float minHeight;
    float maxHeight;
    std::array<LegacyMclqVertex, 81> vertices;
    std::array<uint8_t, 64> cellFlags;
    std::array<uint8_t, 84> flowData;
};

struct LegacyMclqBlock
{
    std::array<std::optional<LegacyMclqRecord>, 4> records;
    uint32_t mcnkLiquidFlags;
};
```

Writer 永远按 River/Ocean/Magma/Slime 的固定 slot 顺序写出。

---

## 6. Same-category merge policy

真实客户端每种 category bit 只消费一个 record，因此**同一类别的多个 MH2O instance 必须先归并成该类别的一份 record**。

允许无损：

- same-category instances 使用不重叠 cells；
- 共享顶点高度/payload 一致；
- 重叠 cells 的数据实际上完全相同，可去重。

需要 diagnostics / lossy fallback：

- same-category 同一 cell 存在不同高度的叠层液体；
- 共享顶点要求不同高度；
- Magma/Slime 同类别内部 payload/UV 冲突。

跨类别：

- Water 与 Ocean 可分别进入两个独立 record；
- Water 与 Magma 即使占同一 cell，也不再因为共享 legacy vertex payload 而发生结构冲突；
- Magma 与 Slime 也分别拥有独立 record，不再是“同一 0x06 selector 无法区分”的结构限制。

因此旧代码中的 `SharedVertexPayloadConflict`、`MagmaSlimeMixed` 只应在**错误的跨类别单-record模型**中出现，重构后不应作为跨类别冲突。

---

## 7. Noggit caveat: do not blindly output one record per MH2O layer

Noggit writer 当前可以按 `_layer_count` 写多份 record，但 Turtle loader 是“每个 category bit 最多消费一次”。因此如果存在两个独立 River source layers，不能简单写两个 River records，因为只有一个 `0x04` category bit。

我们的规则必须比 Noggit 更严格：

```text
MH2O instances
 -> group by target category
 -> merge same-category instances
 -> max one record/category
 -> write categories in fixed bit order
```

---

## 8. Server extractor remains a separate path

**证据等级：Tortoise 源码确认。**

`Penqle/tortoise-wow/tools/extractor/System.cpp` 的 old-MCLQ map extraction 只：

```cpp
adt_MCLQ* liquid = cell->getMCLQ();
```

然后读取第一份 record 的 flags/heights，并根据 MCNK flags 得出 chunk liquid type。它不按四类遍历多个 record。

因此客户端 target ADT 与服务器 map 输入必须继续分离：

```text
CLIENT:
original 335 MH2O
 -> category-grouped multi-record MCLQ
 -> Turtle ADT

SERVER MAP:
original 335 MH2O
 -> Trinity/build12340 liquid reader semantics
 -> Tortoise z1.4 .map writer
```

禁止用 Tortoise old-MCLQ map extractor 对客户端优化后的 multi-record MCLQ 重新生成服务器 `.map`。

---

## 9. warcraft-rs audit result

**证据等级：源码确认。**

`wowemulation-dev/warcraft-rs/file-formats/world-data/wow-adt/src/liquid_converter.rs` 当前不适合作为本项目液体 converter：

- MH2O -> MCLQ 只取 `instances[0]`；
- 只输出一个 `MclqSubchunk`；
- 不实现四 category multi-record；
- MCLQ -> MH2O 方向会假定大范围 render mask 等简化语义；
- 外层 ADT converter 仍有 Placeholder。

保留用途：类型建模、parser/writer架构、validator/测试工程参考。

---

## 10. Required code revision

当前仓库代码需立即重构：

1. `LegacyLiquid.h/.cpp`
   - `LiquidBuildResult` 从单 `optional<LegacyMclq>` 改为 category block；
   - one record/category；
   - `Slime` low code 改为 `0x03`；
   - cross-category overlap allowed；
   - same-category merge/conflict diagnostics。

2. `MclqWriter.h/.cpp`
   - payload serializer仍保持单 record = 804 bytes；
   - block serializer改为 dynamic bytes；
   - raw header = `QLCM + uint32(0)`；
   - append 804-byte records in category order。

3. `McnkWriter.h/.cpp`
   - `optional<LegacyMclq>` -> `LegacyMclqBlock`；
   - `sizeMCLQ = 8 + 804*N`；
   - category flags strictly match record presence。

4. tests
   - Water only: 812 B, flags `0x04`；
   - Water+Ocean: 1616 B, flags `0x0C`；
   - all four: 3224 B, flags `0x3C`；
   - fixed record order；
   - Slime type byte `0x03`；
   - hidden `0x0F`；
   - same-category non-overlap merge；
   - same-category conflicting overlap warning；
   - cross-category overlap allowed。

---

## 11. Current authority order

For legacy MCLQ target behavior:

```text
1. Turtle 1.18.1 real WoW.exe loader
2. Known-good Vanilla fixtures / runtime tests
3. Noggit3 old-MCLQ writer and parser
4. Penqle/tortoise-wow target extractor
5. Trinity/AzerothCore build12340 source semantics
6. warcraft-rs / community abstractions
```

The real target loader is final authority when implementations disagree.
