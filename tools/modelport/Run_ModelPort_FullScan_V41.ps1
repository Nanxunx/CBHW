# ============================================================
# Turtle335Converter Full Golden Scan V4.1
# Uses the Python V4.1 binary scanner to avoid the V4 PowerShell
# per-pair runtime failure that marked all 11,597 pairs as SCAN_ERROR.
# ============================================================

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Scanner = Join-Path $ScriptDir "modelport_fullscan_v41.py"

if (!(Test-Path $Scanner)) {
    throw "找不到扫描器：$Scanner"
}

$Py = $null
$PyArgsPrefix = @()

if (Get-Command py -ErrorAction SilentlyContinue) {
    $Py = "py"
    $PyArgsPrefix = @("-3")
}
elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $Py = "python"
}
elseif (Get-Command python3 -ErrorAction SilentlyContinue) {
    $Py = "python3"
}
else {
    throw "没有找到 Python 3。请先安装 Python 3，或在 Turtle335Converter 的 Python 环境里运行本脚本。"
}

$Source = "E:\335_FinalExtract_V5"
$Target = "E:\335to112_Converted_FinalExtract_V1"
$Out    = "E:\ModelPort_GoldenUpload_ThirdBatch_V41"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Golden Full Scan V4.1" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "335：$Source"
Write-Host "112：$Target"
Write-Host "输出：$Out"
Write-Host ""

$Args = @()
$Args += $PyArgsPrefix
$Args += @(
    $Scanner,
    "--source", $Source,
    "--target", $Target,
    "--out", $Out
)

& $Py @Args

if ($LASTEXITCODE -ne 0) {
    throw "V4.1 扫描失败，Python exit code = $LASTEXITCODE"
}

$Summary = Join-Path $Out "STAGING\00_Metadata\M2_Scan_Summary_V41.txt"
$Zip = Join-Path $Out "ModelPort_GoldenReference_ThirdBatch_V41_ALL.zip"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "V4.1 完成" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green

if (Test-Path $Summary) {
    Get-Content $Summary
}

Write-Host ""
Write-Host "请上传：" -ForegroundColor Yellow
Write-Host $Zip -ForegroundColor White
