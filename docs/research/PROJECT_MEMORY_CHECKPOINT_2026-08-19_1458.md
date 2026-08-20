# 模型移植项目记忆检查点 — 2026-08-19 14:58 +08:00

## 事件

用户运行 `Run_ModelPort_FullScan_V41.ps1`，扫描推进到约 `1750/11597` 后需要携带电脑外出，尝试 Windows 休眠。恢复后 PowerShell/Python 扫描进程没有恢复。

结论：此前建议依赖 Hibernate 保存扫描进程不够稳妥。以后长时间 ModelPort 扫描必须依赖磁盘 checkpoint/resume，而不是依赖 OS 会话恢复。

## V4.1 的限制

`modelport_fullscan_v41.py` 在内存中累计 `rows`，直到全库扫描结束才写最终 CSV。因此被中断的 V4.1 运行无法恢复已经扫描的约1750对；这部分工作需要重新扫描。

## V4.2

新增：

```text
tools/modelport/modelport_fullscan_v42_resume.py
tools/modelport/Run_ModelPort_FullScan_V42_RESUME.ps1
tools/modelport/test_fullscan_v42_resume.py
```

默认输出：

```text
<V42_GOLDEN_ROOT>
```

持久化 checkpoint：

```text
STAGING\00_Metadata\M2_FeatureIndex_335_112_V42.checkpoint.jsonl
```

行为：

1. 每完成一个 335↔112 M2 pair 就追加一条 JSONL。
2. 每条立即 `flush()`，每25条 `fsync()`。
3. 重新运行同一个 V4.2 命令时自动读取 checkpoint。
4. 已完成 RelativePath 自动跳过。
5. 如果 checkpoint 最后一行因异常关机被截断，只忽略该尾行，不丢弃此前结果。
6. 默认绝不删除旧输出；只有显式 `--fresh` 才从0重建。
7. 如果扫描完成但打包阶段中断，重新运行会从完整 checkpoint 直接进入最终输出/打包。

## 当前正确操作

不要再运行 V4.1。

下载/使用 V4.2 两文件并放在同一目录：

```text
Run_ModelPort_FullScan_V42_RESUME.ps1
modelport_fullscan_v42_resume.py
```

然后执行：

```powershell
& "<MODELPORT_WORK_ROOT>\Run_ModelPort_FullScan_V42_RESUME.ps1"
```

如果今后再次需要关机、休眠、重启或进程意外终止，只需重新执行同一命令；不要加 `--fresh`。

## 权威仓库

```text
NansenCore/Turtle335Converter
branch: main
```

后续以本检查点高于 07:02 V4.1 检查点。