# M2 Golden Third-Batch Scan Recovery V4.1

更新时间：2026-08-19 07:02 +08:00

## 输入

用户上传：

`ModelPort_GoldenReference_ThirdBatch_ALL.zip`

原 V4 PowerShell 扫描结果声称：

```text
335 M2 总数：18083
存在成功112同路径 pair：11597
无112同路径：6486
扫描错误：11597

Sequence metadata mismatch：0
AnimationLookup V4 mismatch：0
Playable V4 mismatch：0

Ribbon>0：0
Particle>0：0
TexAnim>0：0
External anim：0
Alias：0
SubAnimation：0
```

## 关键判定

该结果**不能用于全库结论**。

原因很直接：

`扫描错误 = 11597 = paired M2 总数`

也就是原脚本对每一个 pair 都进入了 exception 分支。之后显示的 mismatch=0、Ribbon=0、Particle=0、TexAnim=0、Alias=0 都只是没有成功解析任何 pair 后的假零值。

因此不能据此把 Playable V4 升级成 production baseline，也不能据此说全库没有 Ribbon/Particle。

## 第三批自动选择的 6 个 “V4 mismatch” 是假阳性

第三批 ZIP 实际包含六个被原脚本自动选为 rule mismatch 的 Hellfire doodad：

- Hellfire_gravestones_Horde_01
- Hellfire_gravestones_Alliance_03
- Hellfire_gravestones_Horde_02
- HellfireFloatingRock_Large_01
- Hellfire_gravestones_Horde_03
- Hellfire_gravestones_Alliance_02

对这 6 组 `335 v264 ↔ 成功112 v256` 重新用独立 Python parser 逐二进制解析，结果：

```text
6/6:
scan error = 0
Sequence metadata = exact
AnimationLookup V4 = exact
Playable 226 V4 = exact
Ribbon = 0
Particle = 0
TexAnim = 0
```

它们都只是单 Sequence / AnimID 0 的静态 doodad，不是真正的 metadata mismatch。

所以第三批原来的 selected mismatch 分类全部是 scanner runtime error 产生的假阳性。

## V4.1 处理

新增：

`tools/modelport/modelport_fullscan_v41.py`

核心修复：

1. 直接使用 Python `struct` 做 MD20 二进制解析，明确 bounds check。
2. 每个 pair 的异常写入 `ErrorMessage`，不再只写 `SCAN_ERROR`。
3. Header feature 信息与 rule validation 分离，避免后一步异常把已经读到的 feature 全部变成 -1。
4. 全库扫描前先跑一个 paired M2 preflight；如果 parser/runtime 有问题，立即停止，不再浪费时间扫 11597 个。
5. 新增 `TimelineRulePass`。
6. DBC 打包改成 `00_Metadata/335/...` 与 `00_Metadata/112/...`，修复原 V4 PowerShell 两侧覆盖问题。
7. Feature 选择只基于真正成功解析后的 Header：true Ribbon、Particle、TexAnim、Alias、SubAnimation。
8. Golden Source 继续只读：`<BUILD12340_SOURCE_ROOT>`、`<CLASSIC_GOLDEN_ROOT>`。

## V4.1 回归测试

### 对第三批上传的 6 个假阳性 pair

```text
source M2：6
paired：6
scan errors：0
Sequence mismatch：0
Timeline mismatch：0
AnimationLookup mismatch：0
Playable mismatch：0
```

### 对第二批 13 个已知 Golden pair

```text
source M2：13
paired：13
scan errors：0

Sequence mismatch：0
Timeline mismatch：0
AnimationLookup mismatch：0
Playable mismatch：0

Particle models：7
TexAnim models：3
External .anim models：2
Alias models：3
SubAnimation models：2
Ribbon models：0
```

这与第二批人工/二进制分析结果一致，说明 V4.1 scanner 能正确恢复 feature 信息，而不是全部假零。

## 转换规则状态

第三批失败并没有推翻 Golden V4 转换规则。

目前仍保留：

```text
Sequence.Index = preserve source
Timeline = 每条 sequence 前 +3333ms
AnimationLookup = max(AnimationID)+1
duplicate AnimationID -> prefer SubAnimationID 0
PlayableAnimationLookup = 226
quaternion short -1 -> exact +1.0f
```

第二批 13/13 Playable exact 的事实仍有效。

但 Playable V4 是否能提升成整个 11597-pair 库的 production baseline，必须等 V4.1 全库重扫 `scan_errors=0` 后再裁决。

## 下一步

在用户机器运行：

```powershell
& "<MODELPORT_WORK_ROOT>\Run_ModelPort_FullScan_V41.ps1"
```

输出：

```text
<V41_GOLDEN_ROOT>\
  STAGING\00_Metadata\
    M2_FeatureIndex_335_112_V41.csv
    M2_Scan_Summary_V41.txt
    M2_Scan_Summary_V41.json
    Selected_Samples_V41.csv
    SELECTED_MANIFEST_SHA256_V41.csv

  ModelPort_GoldenReference_ThirdBatch_V41_ALL.zip
```

只有满足 `scan_errors = 0` 时，才继续解释全库 mismatch/feature 统计。
