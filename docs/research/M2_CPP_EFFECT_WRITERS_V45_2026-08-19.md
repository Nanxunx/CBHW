# M2 C++ Effect Writers V4.5 — Ribbon + Particle

日期：2026-08-19

## 目标

把已经通过成功 1.12/Turtle-compatible Golden 文件证明的 Python 算法 oracle 迁移进最终 C++ 转换引擎，而不是继续依赖研究脚本。

## Ribbon

Golden 基线：

```text
6 paired models
420 RibbonEmitters
2520 animation tracks
WotLK fixed stride 176B
Classic/Turtle fixed stride 220B
```

Python oracle：

```text
tools/modelport/ribbon_converter_v45.py
tools/modelport/validate_ribbon_golden_v45.py
```

C++ 生产路径：

```text
include/turtle335/m2/RibbonWriter.h
src/m2/RibbonWriter.cpp
```

本轮修正 C++ Ribbon writer：

- 补全 `std::copy_n` 所需 include；
- color 空 per-sequence track 的语义默认值改为 `(1,1,1)`，不再错误写全0；
- interpolation 2/3 时按 spline value/in-tan/out-tan 计算真实 key size；
- 保持 texture/material array relocation；
- 保持 source `+116..131 -> target +148..163` 静态映射；
- WotLK `priority_plane/padding` 不进入 Classic record。

新增 C++ regression：

```text
tests/test_ribbon_writer.cpp
```

并将 `LegacyTrack.cpp` / `RibbonWriter.cpp` 与该测试正式接入 CMake。

## Particle V2

Golden Python oracle：

```text
tools/modelport/particle_v2.py
```

已知 selected Golden 覆盖：

```text
10 paired models
772 ParticleEmitters
8492 versioned animation tracks
WotLK fixed stride 476B
Classic/Turtle successful target stride 504B
```

新增 C++：

```text
include/turtle335/m2/ParticleWriter.h
src/m2/ParticleWriter.cpp
tests/test_particle_writer.cpp
```

当前 C++ V2 writer 已实现：

- `target flags = source flags & 0xFFFF`；
- model/particle filename relocation；
- WotLK u8 blend/emitter -> Classic u16 fields；
- 10 个 float animation tracks 的 20B -> 28B legacy conversion；
- color/alpha/size FakeAnimBlock 降级；
- head/tail cell 固定十项降级；
- final target physical tail offsets：332/336/348/360/380/392/404/408/412/420/432/444/452/468/476；
- Rot2 signed `-0.0 -> +0.0`；
- Trans 仅保留 X/Y；
- empty enabled track 合成 legacy `Times=[0], Keys=[1]`；
- `nUnknownReference != 0` 明确拒绝，禁止无 Golden 证据猜测。

`ParticleWriter.cpp` 和 regression 已接入 CMake。

## 安全状态

```text
Ribbon Python Golden oracle       GOLDEN_OFFLINE_READY
Ribbon C++ writer                 INTEGRATED / BUILD-TEST PENDING
Particle Python V2 oracle         GOLDEN_SELECTED_READY
Particle C++ writer               INTEGRATED / BUILD-TEST PENDING
Particle nonzero unknown-ref      BLOCKED
```

GitHub Actions `core-tests.yml` 对 main push 同时运行 Ubuntu + Windows CMake build/ctest；本轮提交后的 CI 结果需要继续核验，不能在通过前宣称 C++ production-ready。

## 用户侧唯一并行输入

无需再提供普通模型，也不要再跑全库扫描。只需要运行：

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

上传：

```text
<V45_GOLDEN_ROOT>\ModelPort_GoldenReference_V45_Refine_ALL.zip
```

该包只包含旧498 Playable failure 的 V4.5 精确重验和 29/27 Sequence/Timeline exceptions。

收到后冻结剩余动画元数据规则，并开始 whole-M2 v256 serializer 组合。
