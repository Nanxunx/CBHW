# M2 Golden Reference V4.5 — Playable selected recovery + Ribbon writer

日期：2026-08-19

## V4.4 基线

```text
source M2                  18083
paired M2                  11596
deep high-risk              5036
deep errors                    0
Sequence mismatch              29
Timeline mismatch              27
AnimationLookup mismatch        0
Playable V4 mismatch          498
true Ribbon models            268
Particle models              5570
```

本检查点不重新广扫全库，只使用 V4.4 已打包的高价值 Golden 样本。

## Playable V4.5：8 个 selected V4 failures 已全部解释

V4.4 `01_RuleMismatch` 的 8 个成功 target：NetherDrakeElite、NorthrendNetherDrake、NetherDrake、Kaelthas、CrystalSatyr、CelestialDragonWyrm、TeronGorefiend_Mounted、Kologarn。

旧 V4 主要差异：

```text
40  : predicted 0 -> Golden 41
121 : predicted 0 -> Golden 14
170/171/173/176/178/179/181:
旧 V4 过早直接落到16；Golden 根据模型已有动画解析为19/17/16。
```

当前 `playable_lookup_v45.py` 使用：

```text
build12340 AnimationData.dbc field 5
+ Golden overrides
+ model-aware recursive fallback
```

对上述 8/8 模型全部 226 records：`mismatch = 0`。

仍需本机运行 `Run_ModelPort_Refine_V45.ps1` 对原 498 paths 做精确重验，才能冻结剩余 fallback graph。

## Ribbon：420 emitters / 2520 tracks Golden pass

V4.4 `02_TrueRibbon` 6组：

```text
Arthas_Souls_Attack                        122
Auchindoun_Bridge_Spirits_Flying          121
IceCrown_Frostmourne_Altar_Effect          98
Icecrown_throne_exteriorspires             33
Icespire_FX                                28
Auchindoun_Bridge_Spirits_Floating         18
------------------------------------------------
总计                                       420 emitters
```

固定 record size：

```text
WotLK v264 RibbonEmitter       176 bytes
Classic/Turtle v256            220 bytes
```

420/420 static mapping：

```text
source +0..19 -> target +0..19       id/bone/position
texture_indices payload              exact
material_indices payload             exact
source +116..131 -> target +148..163 edges/lifetime/gravity/rows/cols
```

WotLK `priority_plane + padding` 不进入 Classic v256 record。

每个 Ribbon 六个 track：color、alpha、height_above、height_below、tex_slot、visibility。统一使用 `legacy_track_codec_v45.py` 的 WotLK 20B nested track -> Classic 28B Ranges/Times/Keys 转换。420 * 6 = 2520 tracks，逐项与成功112 target 比较：`2520/2520 PASS`。

新增 `tools/modelport/ribbon_converter_v45.py`。从 source 真实生成 self-contained Classic Ribbon block，再解析与 Golden target 对比：420/420 emitters 与 2520/2520 tracks semantic match。

因此 Ribbon Python oracle 状态升级为：

```text
GOLDEN_OFFLINE_READY
```

下一步不是继续猜 Ribbon，而是移植到 C++ whole-M2 writer，再只做一个 Turtle 1.18.1 Ribbon 实机 regression。

## 当前需要用户提供

运行：

```text
tools/modelport/Run_ModelPort_Refine_V45.ps1
```

上传：

```text
E:\ModelPort_GoldenUpload_V45_Refine\ModelPort_GoldenReference_V45_Refine_ALL.zip
```

该脚本只处理 V4.4 已知旧498 Playable failures + 29/27 Sequence/Timeline exceptions，不再广扫18083/11596/5036。

收到后：冻结 Playable -> 分类 Sequence/Timeline 特例 -> C++ Ribbon -> C++ Particle V2 -> whole-M2 writer -> validator -> 最少量真实客户端回归。
