# Turtle335Converter V4.6 Light Golden collector
$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $Here "Collect_Light_Golden_V46.py"

$Src335 = "E:\335_FinalExtract_V5"
$Src112 = "E:\335to112_Converted_FinalExtract_V1"
$Out    = "E:\ModelPort_LightGolden_V46"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Light Golden V4.6" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "只读 M2 Header；只打包最多5个真正 Light>0 的335/112同路径pair。" -ForegroundColor Cyan
Write-Host "不是全库深扫描。" -ForegroundColor Yellow

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

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out --limit 5
} else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out --limit 5
}
if ($LASTEXITCODE -ne 0) { throw "Light Golden collector failed: $LASTEXITCODE" }

Write-Host ""
Write-Host "完成：" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_Light_V46_ALL.zip") -ForegroundColor White
