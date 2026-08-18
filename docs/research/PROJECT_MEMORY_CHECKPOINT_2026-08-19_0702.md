# 模型移植项目记忆检查点 — 2026-08-19 07:02 +08:00

## 当前权威仓库

`NansenCore/Turtle335Converter` / `main`

## 第三批实际结果

用户上传 `ModelPort_GoldenReference_ThirdBatch_ALL.zip`。

原 V4 PowerShell 扫描报告：

```text
335 total = 18083
paired = 11597
scan_errors = 11597
```

所以原报告中的 Sequence mismatch=0、AnimationLookup mismatch=0、Playable mismatch=0、Ribbon=0、Particle=0、TexAnim=0、Alias=0、SubAnim=0 全部不能作为全库结论，因为没有任何 pair 完整跑完扫描流程。

## 六个自动 selected mismatch

第三批只实际打包了 6 个 Hellfire doodad。

独立重新解析：

```text
6/6 scan OK
6/6 Sequence exact
6/6 AnimationLookup exact
6/6 Playable 226 exact
```

这些都是原 scanner error 的假阳性，不是真 V4 mismatch。

## V4.1 scanner

新增：

```text
tools/modelport/modelport_fullscan_v41.py
tools/modelport/Run_ModelPort_FullScan_V41.ps1
```

改用 Python struct parser + bounds check。

必须先做 preflight；若首个 pair 失败直接中止。

每个 row 新增 `ErrorMessage` 和 `TimelineRulePass`。

DBC 两侧分目录打包，修正旧脚本覆盖问题。

## V4.1 本地回归

第三批 6 false positives：

```text
scan_errors=0
all 6 V4 metadata exact
```

第二批13 Golden：

```text
scan_errors=0
Sequence mismatch=0
Timeline mismatch=0
AnimationLookup mismatch=0
Playable mismatch=0
Particle models=7
TexAnim models=3
External anim models=2
Alias models=3
SubAnimation models=2
Ribbon models=0
```

与既有人工分析一致。

## 转换规则状态

保持 V4：

- Sequence.Index preserve source
- timeline per sequence +3333
- AnimationLookup maxID+1 / missing 0xFFFF / prefer sub0
- Playable 226 + Golden V4 fallback graph
- quaternion -1 -> exact +1.0
- legacy Range/Times/Keys validated
- external skin -> embedded View validated

但 **Playable production baseline 仍未通过全库 gate**。

Gate：

```text
V4.1 full scan:
scan_errors == 0
```

然后才检查：

```text
Playable V4 mismatch == 0 ?
```

## 下一步

用户机器重新运行 V4.1 全库扫描，上传：

`E:\ModelPort_GoldenUpload_ThirdBatch_V41\ModelPort_GoldenReference_ThirdBatch_V41_ALL.zip`

如果全库 scanner 正常，下一阶段自动获得真正 Ribbon / Particle / Alias/SubAnimation Golden 样本。
