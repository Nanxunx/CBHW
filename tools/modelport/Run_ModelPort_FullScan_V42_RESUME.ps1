# Turtle335Converter Golden Full Scan V4.2 - checkpoint/resume
$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py   = Join-Path $Here "modelport_fullscan_v42_resume.py"

$Src335 = "E:\335_FinalExtract_V5"
$Src112 = "E:\335to112_Converted_FinalExtract_V1"
$Out    = "E:\ModelPort_GoldenUpload_ThirdBatch_V42"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Golden Full Scan V4.2 RESUME" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "335：$Src335"
Write-Host "112：$Src112"
Write-Host "输出：$Out"
Write-Host ""
Write-Host "每个模型pair都会写入checkpoint；中断后重复运行本命令即可续扫。" -ForegroundColor Cyan
Write-Host "不要使用 --fresh，除非你确定要从0重新扫描。" -ForegroundColor Yellow
Write-Host ""

$Python = $null
foreach ($Candidate in @("py", "python", "python3")) {
    try {
        $Cmd = Get-Command $Candidate -ErrorAction Stop
        if ($Candidate -eq "py") {
            & $Candidate -3 --version *> $null
            if ($LASTEXITCODE -eq 0) { $Python = @($Candidate, "-3"); break }
        } else {
            & $Candidate --version *> $null
            if ($LASTEXITCODE -eq 0) { $Python = @($Candidate); break }
        }
    } catch {}
}

if ($null -eq $Python) {
    throw "找不到 Python 3。"
}

if (!(Test-Path $Py)) {
    throw "找不到扫描器：$Py"
}

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out
} else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out
}

if ($LASTEXITCODE -ne 0) {
    throw "V4.2 扫描器退出码：$LASTEXITCODE"
}

Write-Host ""
Write-Host "完成。总包：" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_ThirdBatch_V42_ALL.zip") -ForegroundColor White
