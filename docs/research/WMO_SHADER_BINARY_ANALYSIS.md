# WMO Shader Binary Analysis — 3.3.5a vs Turtle/Vanilla

状态：**Binary-backed checkpoint**

目标：记录 3.3.5a build 12340 与 Turtle WoW 1.18.1 客户端真实 WMO 材质加载行为，为 `WmoMaterialRetroporter` 提供可实现规则。

> 证据等级：本页中的地址/步长/分支结论优先来自用户提供的真实 `WoW.exe` 二进制；源码和外部逆向资料仅作为交叉验证。

---

## 1. 3.3.5a MOMT 记录

客户端：WoW 3.3.5a build 12340 (`Wow(1).exe`)

MOMT stride：

```text
0x40 = 64 bytes
```

材质加载函数：

```text
0x7D7710
```

确认读取：

```text
MOMT + 0x04 = shader
MOMT + 0x0C = texture0 MOTX offset
MOMT + 0x18 = texture1 MOTX offset
```

### 1.1 Shader texture-count grouping

真实客户端跳表/分支确认：

```text
Shader 0 -> Texture0 only
Shader 1 -> Texture0 only
Shader 2 -> Texture0 only
Shader 3 -> Texture0 + Texture1
Shader 4 -> Texture0 only
Shader 5 -> Texture0 + Texture1
Shader 6 -> Texture0 + Texture1
```

因此：

```text
single-texture family = {0,1,2,4}
dual-texture family   = {3,5,6}
```

对于 shader 3/5/6，如果第二纹理名为空，3.3.5a 客户端会把材质退化为 shader 4 的单纹理路径。

---

## 2. Shader 6 是 WotLK extended-vertex material

在 3.3.5a group/material 组装路径约：

```text
0x7D8532
```

客户端遍历 MOBA batch，通过 batch material index 找到对应 64-byte MOMT，然后比较：

```text
MOMT.shader == 6
```

若任意 batch 使用 shader 6，则约在：

```text
0x7D8561
```

设置 group runtime flag：

```text
[group + 0x198] |= 0x8
```

该 flag 随后在至少：

```text
0x7C9CB5
0x7CBCBC
```

被消费。

### 2.1 Runtime vertex stride

`0x7CBCBC` 附近根据 runtime flag 0x8 选择：

```text
flag clear -> 0x24 bytes/vertex
flag set   -> 0x30 bytes/vertex
```

差值：

```text
0x30 - 0x24 = 0x0C = 12 bytes
```

这与 WotLK second sets 完全吻合：

```text
second MOCV = 4 bytes/vertex
second MOTV = 8 bytes/vertex
-------------------------------
total       = 12 bytes/vertex
```

### 2.2 Extended vertex builder

函数约：

```text
0x7C8560
```

可见 extended path 按 `0x30` stride 写顶点，并包含：

- position/normal
- first vertex color
- optional second vertex color
- first UV
- optional second UV

因此当前结论：

> **Shader 6 是 3.3.5a 中明确触发第二套 MOCV + 第二套 MOTV runtime vertex layout 的关键 shader。**

证据等级：**真实 3.3.5a 客户端二进制确认**。

---

## 3. WotLK group second-set flags

3.3.5a loader 已确认：

```text
MOGP 0x01000000 -> second MOCV / CVERTS2
MOGP 0x02000000 -> second MOTV / TVERTS2
```

元素尺寸：

```text
MOCV2 = 4 bytes/vertex
MOTV2 = 8 bytes/vertex
```

Turtle/Vanilla old loader 没有相同的 WotLK high-bit second-set load path。

因此目标写出时不能 blind-copy 这两个 feature。

---

## 4. Turtle/Vanilla MOMT baseline

Turtle WoW 1.18.1 客户端：

```text
MOMT stride = 0x40 = 64 bytes
```

材质加载函数约：

```text
0x6C3DB0
```

明确解析：

```text
MOMT + 0x0C -> texture0
MOMT + 0x18 -> texture1
```

并写入 runtime texture handles：

```text
+0x38
+0x3C
```

未发现第三纹理加载路径。

独立 Vanilla 1.12.1 客户端逆向资料 `samwhosung/wow-1121-client-internals` 的 WMO 结论与此一致。

### 4.1 Important unresolved point

Turtle WMO render 路径约 `0x6B4F82` 会读取 MOMT flags/shader，并存在：

```text
MOMT.shader == 1
```

的特殊分支。

这证明 Vanilla/Turtle 也有 shader 语义，但**不能假定其数字 ID 与 WotLK shader 0..6 一一相同**。

当前禁止执行：

```text
WotLK shader N -> Turtle shader N
```

除非逐 ID runtime 行为已经验证。

---

## 5. Converter tiers — now binary-backed

### Tier A — WotLK shaders 0 / 1 / 2 / 4

已确认：单纹理 family。

输入需求：

```text
Texture0
MOTV1
```

处理策略：

- 保留主纹理和第一 UV。
- 根据后续 Turtle shader 映射选择目标 shader。
- 清理 WotLK-only material/group flags。
- blend/lighting 仍需语义验证。

### Tier B — WotLK shaders 3 / 5

已确认：双纹理 family，但当前没有证据表明它们像 shader 6 一样强制使用 second MOCV/MOTV extended vertex layout。

处理策略：

- 保留 Texture0 + Texture1。
- 优先尝试映射到 Vanilla/Turtle legacy dual-texture path。
- 如果 Turtle 没有等价语义，则进入 texture bake。

### Tier C — WotLK shader 6

已确认：双纹理 + extended vertex path；关联 second MOCV + second MOTV。

处理策略：

1. 若第二 UV/color 可证明冗余 -> 折叠。
2. 否则执行 offline material/texture bake。
3. 输出只保留 Vanilla 可表达的单 set。
4. 删除 second MOCV/MOTV chunks。
5. 清除：

```text
MOGP 0x01000000
MOGP 0x02000000
```

6. 记录 `lossy` / `baked` diagnostics。

禁止静默删除第二 UV/color 后报告无损成功。

---

## 6. Required next binary work

仍需在 Turtle 1.18.1 客户端真实 render/material 路径确认：

- Turtle shader 0 的 exact bind behavior
- Turtle shader 1 的 exact bind behavior
- 是否存在第二个 legacy dual-texture shader
- Texture0/Texture1 各自如何绑定 stage
- blend mode 与 shader 的组合规则
- 是否存在 env-map / specular 等价路径

最终目标是建立：

```text
WotLK shader 0..6
        -> semantic material model
        -> Turtle shader / bake strategy
```

而不是数字 ID 直映射。

---

## 7. Evidence rule

后续所有 WMO shader 映射按下列等级记录：

1. Turtle/335 real `WoW.exe` binary confirmed
2. Client-source/reverse-engineering corpus corroborated
3. Server extractor/source confirmed
4. Strong inference
5. Unverified / runtime test required

最终裁判：**Turtle 1.18.1 real client rendering + Tortoise VMAP extraction + in-game visual/collision regression tests**.
