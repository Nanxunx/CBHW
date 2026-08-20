param(
    [Parameter(Mandatory=$true)]
    [string]$Src335,

    [Parameter(Mandatory=$true)]
    [string]$Src112,

    [Parameter(Mandatory=$true)]
    [string]$Deep,

    [Parameter(Mandatory=$true)]
    [string]$AnimDbc,

    [Parameter(Mandatory=$true)]
    [string]$Out
)

# Turtle335Converter V4.5 focused refinement
$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $Here "modelport_refine_v45.py"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter ModelPort Refine V4.5" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "只重新检查 V4.4 的 498 个 Playable mismatch。" -ForegroundColor Cyan
Write-Host "同时自动打包 29/27 个 Sequence/Timeline exception。" -ForegroundColor Cyan
Write-Host "不会重新扫描 18083 / 11596 个模型。" -ForegroundColor Yellow
Write-Host ""

$Python = $null
foreach ($Candidate in @("py","python","python3")) {
    try {
        Get-Command $Candidate -ErrorAction Stop | Out-Null
        if ($Candidate -eq "py") {
            & $Candidate -3 --version *> $null
            if ($LASTEXITCODE -eq 0) { $Python=@($Candidate,"-3"); break }
        } else {
            & $Candidate --version *> $null
            if ($LASTEXITCODE -eq 0) { $Python=@($Candidate); break }
        }
    } catch {}
}
if ($null -eq $Python) { throw "找不到 Python 3" }
if (!(Test-Path $Py)) { throw "找不到：$Py" }

$Args = @(
    "--source", $Src335,
    "--target", $Src112,
    "--v44-deep", $Deep,
    "--animation-data", $AnimDbc,
    "--out", $Out
)

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py @Args
} else {
    & $Python[0] $Py @Args
}
if ($LASTEXITCODE -ne 0) { throw "V4.5 refinement failed: $LASTEXITCODE" }

Write-Host ""
Write-Host "完成，总包：" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_V45_Refine_ALL.zip") -ForegroundColor White
