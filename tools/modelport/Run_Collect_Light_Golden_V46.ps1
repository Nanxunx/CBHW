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
$Py = Join-Path $Here "Collect_Light_Golden_V46.py"

if (!(Test-Path -LiteralPath $Py -PathType Leaf)) {
    throw "Light Golden collector not found: $Py"
}

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Light Golden V4.6" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Collect up to 5 paired models with non-zero Light records." -ForegroundColor Cyan
Write-Host "This is not a full deep scan." -ForegroundColor Yellow
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

if ($Python.Count -eq 2) {
    & $Python[0] $Python[1] $Py --source $Src335 --target $Src112 --out $Out --limit 5
}
else {
    & $Python[0] $Py --source $Src335 --target $Src112 --out $Out --limit 5
}

if ($LASTEXITCODE -ne 0) {
    throw "Light Golden collector failed. Exit code = $LASTEXITCODE"
}

Write-Host ""
Write-Host "Finished. Evidence ZIP:" -ForegroundColor Green
Write-Host (Join-Path $Out "ModelPort_GoldenReference_Light_V46_ALL.zip") -ForegroundColor White