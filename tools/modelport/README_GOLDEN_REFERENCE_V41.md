# Golden Full Scan V4.1

V4.1 是扫描器修复，不是新的私有模型格式。

最终模型输出规则仍是：

```text
MD20 v256
Classic embedded View
标准 BLP / DBC
```

## 为什么需要 V4.1

原 `Pack_ModelPort_GoldenReference_ThirdBatch_AutoScan.ps1` 的第三批输出出现：

```text
paired = 11597
scan_errors = 11597
```

所以原来的全库 `mismatch=0 / Particle=0 / Ribbon=0` 都无效。

V4.1 改用 Python 二进制 parser，并在全库扫描前做 preflight。

## 运行

把这两个文件放在同一目录：

```text
modelport_fullscan_v41.py
Run_ModelPort_FullScan_V41.ps1
```

PowerShell：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
& ".\Run_ModelPort_FullScan_V41.ps1"
```

默认读取：

```text
<BUILD12340_SOURCE_ROOT>
<CLASSIC_GOLDEN_ROOT>
```

默认输出：

```text
<V41_GOLDEN_ROOT>
```

## 裁决门槛

先看：

```text
scan_errors
```

只有它为 0，才允许用后面的：

- Sequence mismatch
- AnimationLookup mismatch
- Playable mismatch
- Ribbon / Particle / TexAnim
- Alias / SubAnimation

作为全库结论。
