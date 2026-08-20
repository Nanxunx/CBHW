param(
    [Parameter(Mandatory=$true)]
    [string]$Src335,

    [Parameter(Mandatory=$true)]
    [string]$Src112,

    [Parameter(Mandatory=$true)]
    [string]$Out
)

$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Py = Join-Path $Here "modelport_fullscan_v42_resume.py"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Golden Full Scan V4.2 RESUME" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Source: $Src335"
Write-Host "Target: $Src112"
Write-Host "Out   : $Out"
Write-Host ""
Write-Host "Checkpoint/resume mode is enabled." -ForegroundColor Cyan
Write-Host "Do not use --fresh unless a complete restart is intended." -ForegroundColor Yellow
Write-Host ""

$Python = $null

foreach ($Candidate in @("py", "python", "python3")) {
    try {
        Get-Command $Candidate -ErrorAction Stop | Out-Null

        if ($Candidate -eq "py") {
            & $Candidate -3 --version *> $null
            if ($LASTEXITCODE -eq 0) {
                $Python = @($Candidate, "-3")
                break
            }
        }
        else {
            & $Candidate --version *> $null
            if ($LASTEXITCODE -eq 0) {
                $Python = @($Candidate)
                break
            }
        }
    }
    catch {}
}

if ($null -eq $Python) {
    throw "Python 3 was not found."
}

if (!(Test-Path -LiteralPath $Py -PathType Leaf)) {
    throw "Scanner not found: $Py"
}

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out
}
else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out
}

if ($LASTEXITCODE -ne 0) {
    throw "V4.2 scanner failed. Exit code = $LASTEXITCODE"
}

Write-Host ""
Write-Host "Finished. Evidence ZIP:" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_ThirdBatch_V42_ALL.zip") -ForegroundColor White