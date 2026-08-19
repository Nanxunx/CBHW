# 模型移植项目 — 当前权威记忆

更新时间：2026-08-19 14:58 +08:00

本文件是 `NansenCore/Turtle335Converter` 的当前记忆入口。

## 最新 M2 / ModelPort 检查点

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_1458.md
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-19_0702.md
docs/research/M2_THIRD_BATCH_SCAN_RECOVERY_V41_2026-08-19.md
docs/research/M2_GOLDEN_REFERENCE_SECOND_BATCH_V4_2026-08-19.md
```

## 当前扫描器

V4.1 二进制解析逻辑仍是当前基础，但长扫描执行入口已升级为 V4.2 checkpoint/resume：

```text
tools/modelport/modelport_fullscan_v42_resume.py
tools/modelport/Run_ModelPort_FullScan_V42_RESUME.ps1
```

原因：用户实机 V4.1 扫描到约 `1750/11597` 后进入 Windows 休眠，恢复后 Python/PowerShell 进程没有恢复。V4.1 直到扫描结束才写最终 CSV，因此此前约1750条无法从磁盘恢复。

V4.2 持久化：

```text
E:\ModelPort_GoldenUpload_ThirdBatch_V42\
STAGING\00_Metadata\M2_FeatureIndex_335_112_V42.checkpoint.jsonl
```

每完成一对 M2 即追加 checkpoint；重新运行同一命令自动跳过已完成 RelativePath。不要依赖 Hibernate 保存扫描进程，也不要使用 `--fresh`，除非明确要从0重新开始。

## V4.1 已确认基础

第三批原 PowerShell 全库扫描的 `scan_errors=11597` 是扫描器故障，不是模型规则失败。自动挑出的6个 mismatch 经 V4.1 独立解析全部是假阳性。

V4.1 已在：

```text
第三批6个假阳性 pair
+
第二批13个 Golden pair
```

共19个 pair 回归通过：

```text
scan_errors = 0
Sequence mismatch = 0
Timeline mismatch = 0
AnimationLookup mismatch = 0
Playable mismatch = 0
```

## Production gate

Playable V4 第二批 13/13 exact 仍有效，但尚不能宣告整个约11597-pair 库 production-ready。

必须完成 V4.2 全库扫描并满足：

```text
scan_errors == 0
```

然后检查：

```text
Playable V4 mismatch == 0
```

## 转换规则

保持 Golden V4：

```text
MD20 v264 -> standard MD20 v256
Sequence.Index = preserve source
AnimationLookup = max(AnimationID)+1
duplicate ID -> prefer SubAnimationID 0
Playable = 226
quaternion -1 -> exact +1.0
skin -> embedded View
```

Golden Source 两个目录都只读：

```text
E:\335_FinalExtract_V5
E:\335to112_Converted_FinalExtract_V1
```

## ADT

ADT 详细记忆仍从：

```text
docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md
```

读取。

## 权威仓库

```text
NansenCore/Turtle335Converter
branch: main
```

`WYTurtle` 不是模型转换器源码仓库。
