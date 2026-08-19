# Turtle335Converter Focused Golden Scan V4.4
$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $Here "modelport_targeted_scan_v44.py"

$Src335 = "E:\335_FinalExtract_V5"
$Src112 = "E:\335to112_Converted_FinalExtract_V1"
$Out    = "E:\ModelPort_GoldenUpload_Targeted_V44"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Focused Golden Scan V4.4" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Phase 1：全库仅读 Header" -ForegroundColor Cyan
Write-Host "Phase 2：仅对 animated 模型读取 source Sequence 表做风险分类" -ForegroundColor Cyan
Write-Host "Phase 3：只深扫 Ribbon / Particle / TexAnim / external anim / alias / subanim / duplicate AnimID" -ForegroundColor Cyan
Write-Host "额外只抽样 24 个普通动画模型做回归，不再深扫 11597 对。" -ForegroundColor Yellow
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
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out --animation-regression-limit 24
} else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out --animation-regression-limit 24
}

if ($LASTEXITCODE -ne 0) { throw "V4.4 扫描器退出码：$LASTEXITCODE" }

Write-Host ""
Write-Host "完成，总包：" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_Targeted_V44_ALL.zip") -ForegroundColor White
