# Wallcraft M2 Workshop 对 Turtle335Converter 的价值分析

日期：2026-08-19

参考仓库：

```text
https://github.com/Wall-core/M2Workshop
master @ 05d4ee64b51711d7c673a8ab003e18537ab2ada7
```

目标仓库：

```text
NansenCore/Turtle335Converter
branch: main
```

## 结论

Wallcraft M2 Workshop **非常有帮助，但它不是 3.3.5a -> Turtle/Vanilla 的降版转换器**。

它的最高价值是：

1. 给出了一个近年维护的、覆盖 Vanilla/TBC/WotLK 的 M2 固定结构/track offset 表；
2. 实际遍历 Bone/Color/Transparency(Texture Weight)/Texture Transform/Attachment/Event/Light/Camera/Ribbon/Particle；
3. 明确展示 WotLK nested track 与 legacy Range/Times/Keys track 的版本分界；
4. 最重要：实现 WotLK `.anim` sidecar 自动发现和按 sequence 加载；
5. 独立确认 Vanilla bone rotation 使用 float C4Vector，而 WotLK/TBC 使用压缩 short quaternion 路径。

它不能替代我们现有 Golden：

- 不处理 `.skin -> embedded View`；
- 不生成 `AnimationLookup`；
- 不生成 canonical `PlayableAnimationLookup=226`；
- 不重建 324-byte Turtle/Classic header；
- 不做 DBC/MPQ；
- 是 010 Editor 内的**同版本编辑/变换脚本**，不是跨版本 serializer。

因此证据优先级继续保持：

```text
真实 Turtle/成功112 Golden
> 我们的大样本 paired corpus
> Wallcraft M2 Workshop / Coffee 等独立实现
> Wiki/历史备注
```

## 仓库内容与维护状态

M2Workshop 根目录只有：

```text
README.md
Wallcraft M2 Workshop.1sc
```

`.1sc` 约 475 KB。脚本内部版本为 `1.0.21`，历史记录包括：

```text
2025-04-15 .anim autoload
2025-04-18 subtracks
2025-04-19 ribbon tracks
2025-04-20 particle tracks
2025-04-26 missing WotLK+ particle vars
2025-05-06 Vanilla bone rotation -> C4Vector
2026-04-27 typo fixes
```

GitHub 最新提交为 2026-05-02 的 typo fix。

根目录未观察到 LICENSE 文件，因此本项目**不复制/内嵌其 .1sc 实现**；只把公开可验证的二进制格式事实作为独立实现的交叉证据。

## 与 Golden 一致的固定结构矩阵

Wallcraft 对 v264(WotLK) 和 legacy/Vanilla 表给出的尺寸，与我们 Golden 的核心块高度一致：

| Block | WotLK v264 | Classic/Turtle writer |
|---|---:|---:|
| Sequence | 64 | 68 |
| Bone | 88 | 108 |
| Color | 40 | 56 |
| Transparency/Weight | 20 | 28 |
| TextureAnimation/Transform | 60 | 84 |
| Attachment | 40 | 48 |
| Event | 36 | 44 |
| Light | 156 | 212 |
| Camera | 100 | 124 |
| Ribbon | 176 | 220 |
| Particle | 476 | 504 |
| WotLK AnimationTrack | 20 | - |
| Classic AnimationBlock | - | 28 |

注意：脚本把 `version==256` 文案标成 `Alpha`，把 257..259 标成 `Vanilla`。**这个命名不能作为我们的目标版本判据**。我们的真实 Turtle/成功112 Golden 已直接证明目标 MD20 是 v256，并且固定结构与这里的 legacy 表相符。Golden 高于脚本文案。

## 关键 offset 交叉验证

### WotLK v264

```text
Bone 88:
  translation 0x10
  rotation    0x24
  scaling     0x38
  pivot       0x4C

Color 40:
  rgb         0x00
  alpha       0x14

TexAnim 60:
  translation 0x00
  rotation    0x14
  scaling     0x28

Attachment 40:
  position    0x08
  enabled     0x14

Event 36:
  position    0x0C
  timer       0x18

Light 156:
  position          0x04
  ambientColor      0x10
  ambientIntensity  0x24
  diffuseColor      0x38
  diffuseIntensity  0x4C
  attenuationStart  0x60
  attenuationEnd    0x74
  visibility        0x88

Camera 100:
  FOV          0x04
  far          0x08
  near         0x0C
  transPos     0x10
  position     0x24
  transTarget  0x30
  target       0x44
  roll         0x50
```

### Classic/Turtle legacy target

```text
Bone 108:
  translation 0x0C
  rotation    0x28
  scaling     0x44
  pivot       0x60

TexAnim 84:
  translation 0x00
  rotation    0x1C
  scaling     0x38

Attachment 48:
  position    0x08
  enabled     0x14

Event 44:
  position    0x0C
  timer       0x18

Light 212:
  position          0x04
  ambientColor      0x10
  ambientIntensity  0x2C
  diffuseColor      0x48
  diffuseIntensity  0x64
  attenuationStart  0x80
  attenuationEnd    0x9C
  visibility        0xB8

Camera 124:
  FOV          0x04
  far          0x08
  near         0x0C
  transPos     0x10
  position     0x2C
  transTarget  0x38
  target       0x54
  roll         0x60
```

Ribbon 176->220、Particle 476->504 也与我们已经通过 Golden 的 C++ writer 一致。

## `.anim` sidecar — 本次最重要的新输入

Wallcraft 明确展示 WotLK+ track 的第二层 ArrayRef 仍在主 M2 中，但当 sequence 不满足 `flags & 0x20` 时，inner payload 应从：

```text
<ModelName><AnimationID:04>-<SubAnimationID:02>.anim
```

打开读取。

例如：

```text
ModelName0069-00.anim
```

其逻辑是：

```text
outer Times/Keys refs      -> main M2
inner count/offset pair    -> main M2
if !(sequence.flags & 0x20):
    inner offset payload   -> matching .anim sidecar
else:
    inner offset payload   -> main M2
```

这补上了当前 `LegacyTrack.cpp` whole-writer 路径中最重要的一个缺口。我们之前只验证了已知 sidecar 在成功目标中可 byte-identical 复制，但 full serializer 还需要能**解析** sidecar 中的 Bone/Color/etc 动画 payload。

本轮因此新增 `ExternalAnim` resolver，而不是继续广扫模型。

## Quaternion 交叉证据

Wallcraft：

- WotLK/TBC compressed quaternion = 4 x short = 8 bytes；
- Vanilla bone rotation path在 2025-05-06 修正为 float `C4Vector`；
- legacy track仍是 28-byte Range/Times/Keys header。

这独立支持我们 Golden 已确认的：

```text
WotLK key: 4 x int16       (8B)
Classic/Turtle key: 4 x f32 (16B)
```

Wallcraft 的 short<->float helper 只能作为压缩区间的佐证，不能直接覆盖我们的 Golden 公式；特别是 source `-1` 必须输出**精确 +1.0f**，继续以真实成功目标为准。

## 不应直接采用的内容

至少存在一个明显表格 typo：某 legacy particle 分支把 `particleEmissionRateOffset` 写成自身递归相加，而不是 `particleEmissionRatePadding`。另外脚本版本命名(v256=Alpha)与我们的真实目标语境不同。

所以：

```text
不复制表
不复制 writer
不把 Wallcraft 当 target oracle
```

只把它用于：

```text
结构覆盖检查
offset 交叉验证
track 类型确认
external .anim 数据源解析
编辑/缩放功能的未来扩展设计
```

## Wall-core 其他仓库

Wall-core 账户里与 WoW 相关的其他仓库主要是：

```text
AshenWoW
Everlook
VMaNGOS-Default
vmangos-manuals
Everlook-Bugtracker
Wallcraft-bugtracker
```

这些主要是 Vanilla 服务端/内容工程。`AshenWoW` README 明确支持 `1.12.1.5875+` 等客户端，后续做服务端 DBC/客户端 build 兼容时有参考价值；但没有发现它们提供比 M2Workshop 更直接的 M2 binary writer/retroport 代码。

## 对当前开发路线的直接影响

本次分析后路线调整为：

```text
V4.6 Playable canonical226      已收敛
Ribbon C++                      已接入
Particle V2 C++                 已接入
Skin -> embedded View           已接入
Classic 324B Header             已接入
Classic Validator               已接入

新增：External .anim resolver   <- Wallcraft 强证据
新增：Quaternion codec          <- Wallcraft + Golden 双证据

下一步：
Bone / Color / Transparency / TexAnim /
Attachment / Event / Light / Camera
        -> whole-M2 relocation writer
        -> MD20 v256
        -> strict validator
        -> 1 Ribbon + 1 Particle/Creature real client regression
```

Wallcraft M2 Workshop 不改变最终目标：输出只允许标准 Turtle/Vanilla `MD20 v256`，不依赖 010 Editor，也不把第三方脚本作为运行时依赖。
