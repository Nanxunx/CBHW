param(
    [Parameter(Mandatory=$true)]
    [string]$Src335,

    [Parameter(Mandatory=$true)]
    [string]$Src112,

    [Parameter(Mandatory=$true)]
    [string]$Out
)

# Turtle335Converter Targeted Golden Scan V4.3
$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $Here "modelport_targeted_scan_v43.py"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Targeted Golden Scan V4.3" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Phase 1：全库只读 Header（快速分类）" -ForegroundColor Cyan
Write-Host "Phase 2：只深扫 Animation/Ribbon/Particle/TexAnim/Event/ExternalAnim" -ForegroundColor Cyan
Write-Host "静态普通模型不会进行深度 Sequence/Playable 比较。" -ForegroundColor Yellow
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
if (!(Test-Path $Py)) { throw "找不到扫描器：$Py" }

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out
} else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out
}

if ($LASTEXITCODE -ne 0) { throw "V4.3 扫描器退出码：$LASTEXITCODE" }

Write-Host ""
Write-Host "完成，总包：" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_Targeted_V43_ALL.zip") -ForegroundColor White
