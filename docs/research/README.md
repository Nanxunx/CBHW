# Turtle335Converter Research Index

本目录用于保存“模型移植”项目的证据、格式结论和实现决策。

## 当前权威入口

按优先级阅读：

1. `PROJECT_MEMORY_2026-08-18.md`
   - 当前项目总状态、已确认结论、下一步顺序。

2. `ADT_FULL_WRITER_TERRAIN_2026-08-18.md`
   - Full ADT Writer、MCIN、MCVT/MCNR/MCLY/MCAL、Fixture A、CI 证据。

3. `NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md`
   - Turtle 真客户端 + Noggit old-MCLQ multi-record 交叉确认。

4. `WOW_CRUCIBLE_PLACEMENT_AUDIT_2026-08-18.md`
   - MMDX/MMID/MWMO/MWID/MDDF/MODF/MCRF/UID/multi-tile placement。

5. `WALL_CORE_REPO_ANALYSIS.md`
   - M2Workshop、VMaNGOS/Everlook 等项目的用途评级。

## 已被后续证据部分取代的文档

### `ADT_MH2O_MCLQ_CONVERSION.md`

该文件保存了第一阶段 MH2O→MCLQ 推导历史，但以下内容**不得再作为当前实现依据**：

```text
一个 MCNK 只有一份 MCLQ record
不同液体 category 必须共享单一 9x9 legacy grid
Slime cell selector = 0x06
fishable 没有可用 legacy bit
```

后续 Turtle binary + Noggit source 已确认：

```text
one MCNK
 -> optional River 804B
 -> optional Ocean 804B
 -> optional Magma 804B
 -> optional Slime 804B
```

顺序固定：

```text
River -> Ocean -> Magma -> Slime
```

cell codes：

```text
Ocean = 0x01
Slime = 0x03
Water/River = 0x04
Magma = 0x06
Hidden = 0x0F
bit6 = fishable
bit7 = fatigue/deep
```

当前实现一律以 `NOGGIT_MCLQ_CROSSCHECK_2026-08-18.md` 为准。

## 证据标签

所有新研究尽量使用：

```text
客户端二进制确认
源码确认
实现确认
强推断
未验证
```

任何“客户端运行成功”结论必须来自真实目标客户端测试，不能仅由 writer 单测或结构 validator 推导。
