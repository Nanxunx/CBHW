# WDT Writer + Fixture A map skeleton — 2026-08-18

状态：**terrain-only WDT Writer/Validator 已实现；Ubuntu + Windows Debug CI 全绿；WDT+ADT 已作为同一 artifact 生成。真实 Turtle 客户端/Noggit 运行仍未验证。**

## 1. Source-confirmed terrain WDT contract

Penqle/tortoise-wow 与 WoW-Crucible 当前源码共同确认：

```text
MVER version = 18
MPHD payload = 32 bytes = 8 * uint32
MAIN payload = 64 * 64 * 8 = 32768 bytes
```

每个 MAIN entry：

```text
uint32 flags
uint32 asyncId
```

Tile slot：

```text
slot = y * 64 + x
byte offset in MAIN payload = slot * 8
```

terrain tile present：

```text
flags & 1 != 0
```

WoW-Crucible `WdtTileTableService.Create()` 明确使用同一公式。

## 2. Global WMO boundary

`MPHD.flags & 1` 表示 global-WMO WDT profile。

当前 `WdtWriter` 只支持 terrain tile-table profile：

```text
MPHD bit0 = 0
```

遇到 global-WMO flag 直接拒绝，不隐式删除 MWMO/MODF semantics。

## 3. Canonical minimal physical layout

当前 Writer 固定输出：

```text
file byte 0:   REVM, payload 4, version 18
file byte 12:  DHPM, payload 32
file byte 52:  NIAM, payload 32768
file byte 60:  MAIN payload begins
```

完整文件大小：

```text
12 + 40 + 32776 = 32828 bytes
```

不写 global-WMO chunks。

## 4. Implementation

```text
include/turtle335/adt/WdtWriter.h
src/adt/WdtWriter.cpp
tests/test_wdt_writer.cpp
tools/generate_fixture_wdt.cpp
tools/validate_wdt.cpp
```

API：

```cpp
SetWdtTerrainTile(input, x, y, present);
SerializeTerrainWdt(input);
ValidateTerrainWdt(bytes);
```

`SetWdtTerrainTile` 只修改 MAIN flags bit0，并保留其他 flags 和 asyncId。

## 5. Fixture A WDT

Fixture profile：

```text
present tile: (32,32)
all other 4095 terrain tiles absent
MPHD = 8 zero uint32 values
no global WMO
```

CI-generated file：

```text
FixtureA.wdt
size = 32828 bytes
SHA-256 = 1411a96c5e732c2e2645108b962deacfd79497fee97c6aa3e3206c872772b9ea
```

Independent binary inspection confirmed：

```text
REVM  size=4   version=18
DHPM  size=32  payload all zero
NIAM  size=32768
MAIN(32,32): flags=1 asyncId=0
all other tile bit0=0
```

## 6. Latest CI evidence

Workflow run：

```text
run id = 32061245006
head = 741222ba2f1c97059a471ebb155ae0c3a26cacb2
```

Ubuntu：

```text
configure PASS
build PASS
6/6 CTest PASS
generate WDT + ADT PASS
validate both from disk PASS
SHA PASS
artifact upload PASS
```

Windows/MSVC：

```text
configure PASS
build PASS
6/6 CTest PASS
Windows map tools artifact upload PASS
```

Artifacts：

```text
Turtle335Converter-FixtureA
artifact id = 9298214853

Turtle335Converter-Windows-Map-Tools
artifact id = 9298227433
```

## 7. Confidence boundary

### 源码确认 / 实现确认

```text
WDT MVER18
MPHD 32B
MAIN 32768B
MAIN y*64+x order
present bit0
minimal terrain-only WDT writer
WDT validator
cross-platform tests
WDT+ADT deterministic artifact generation
```

### 未验证

```text
Noggit GUI loads FixtureA.wdt + FixtureA_32_32.adt as a real map
Turtle client recognizes a new map named FixtureA
Turtle runtime renders this map
```

Important：仅有 WDT+ADT 不意味着 stock/Turtle client 已认识一个新 map directory。下一阻塞层是 client `Map.dbc` / map binding。

## 8. Next step

审计 Vanilla/Turtle `Map.dbc` 的真实 WDBC record schema与客户端/服务器使用字段，目标不是立即伪造 Map.dbc，而是回答：

```text
最小新增 map entry 需要哪些字段？
Directory 字段在哪？
InstanceType/flags 如何影响加载？
AreaTable / loading screen / minimap 等哪些是硬依赖，哪些只是可选显示？
Turtle 是否扩展了 Vanilla Map.dbc schema？
```

在 schema 未被目标源码/真实文件确认前，不生成 synthetic Map.dbc。
