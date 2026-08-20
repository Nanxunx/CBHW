# WMO MOHD Binary Analysis — root flags vs legacy extractor naming

状态：**Binary-confirmed correction**

## 1. Problem

旧 `Penqle/tortoise-wow` vmap extractor 将 MOHD 最后一个 `uint32` 成员命名为 `liquidType`，而 TrinityCore 3.3.5 将对应字段称为 `flags`。如果把旧变量名当成真实文件语义，会导致 WMO root writer 和液体转换设计错误。

## 2. 3.3.5a real client proof

客户端：WoW 3.3.5a build 12340 (`Wow(1).exe`).

Root WMO parser/runtime path around:

```text
0x7D7D06
```

真实机器码：

```asm
MOV EAX,[EDX+0x120]      ; root MOHD pointer
TEST BYTE PTR [EAX+0x3C],0x08
JNE ...
```

因此 MOHD payload `+0x3C` 被客户端按 **bit field** 消费。

这直接证明：

> **3.3.5a MOHD 最后 4 字节是 flags，不是一个普通 liquid type ID。**

证据等级：真实客户端二进制确认。

## 3. MOHD physical layout used by the real clients

当前可固定的 64-byte payload：

```text
+0x00 nTextures
+0x04 nGroups
+0x08 nPortals
+0x0C nLights
+0x10 nDoodadNames
+0x14 nDoodadDefs
+0x18 nDoodadSets
+0x1C ambientColor
+0x20 WMOID
+0x24 bboxMin.x
+0x28 bboxMin.y
+0x2C bboxMin.z
+0x30 bboxMax.x
+0x34 bboxMax.y
+0x38 bboxMax.z
+0x3C flags
```

Total:

```text
0x40 = 64 bytes
```

## 4. Turtle/Vanilla real client behavior

Turtle WoW 1.18.1 root initialization around:

```text
0x6C3990
```

uses the parsed MOHD pointer and explicitly reads:

```text
MOHD + 0x1C -> ambient color
MOHD + 0x24..0x3B -> bounding box
```

The root pointer remains available, but the WMO subsystem paths inspected so far do not treat `+0x3C` as a scalar liquid ID.

This is consistent with `+0x3C` being a root flags field rather than a liquid type record.

## 5. Source cross-check

- TrinityCore 3.3.5 extractor uses `flags` for the root field.
- `warcraft-rs/wow-wmo` models root header state as WMO flags.
- old Tortoise extractor uses the misleading member name `liquidType`, but its surrounding logic is historical extractor code and must not override real-client binary semantics.

## 6. Converter rule

Do NOT model this as:

```cpp
uint32_t rootLiquidType;
```

Use:

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

    Color32 ambientColor;
    uint32_t wmoId;
    Aabb boundingBox;

    uint32_t sourceFlags;
};
```

Then create an explicit target-flags mapper:

```cpp
uint32_t ConvertRootFlags335ToVanilla(uint32_t sourceFlags);
```

Unknown / unsupported high-version bits must be diagnosed instead of blindly copied.

## 7. Liquid conversion consequence

WMO liquid type conversion must remain based on the **group/MLIQ liquid data plus version-specific interpretation**, not on a fictitious `MOHD.liquidType` scalar.

Recommended shared pipeline:

```text
Group liquid value / MLIQ
        +
source root flags where relevant
        +
LiquidType.dbc when required
        ->
Normalized LiquidCategory
        ->
Vanilla/Turtle group liquid encoding
```

This should share the same semantic liquid mapper used by ADT `MH2O -> MCLQ`.

## 8. Required follow-up

Still determine the exact supported root flag mask for Turtle/Vanilla by tracing each consumer / comparing a Vanilla WMO corpus. Until that is complete, the writer should use an allow-list and warn on unknown source bits.
