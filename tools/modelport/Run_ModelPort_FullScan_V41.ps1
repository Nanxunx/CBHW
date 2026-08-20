param(
    [Parameter(Mandatory=$true)]
    [string]$Source,

    [Parameter(Mandatory=$true)]
    [string]$Target,

    [Parameter(Mandatory=$true)]
    [string]$Out
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Scanner = Join-Path $ScriptDir "modelport_fullscan_v41.py"

if (!(Test-Path -LiteralPath $Scanner -PathType Leaf)) {
    throw "Scanner not found: $Scanner"
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
    throw "Python 3 was not found."
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter Golden Full Scan V4.1" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Source: $Source"
Write-Host "Target: $Target"
Write-Host "Out   : $Out"
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
    throw "V4.1 scanner failed. Python exit code = $LASTEXITCODE"
}

$Summary = Join-Path $Out "STAGING\00_Metadata\M2_Scan_Summary_V41.txt"
$Zip = Join-Path $Out "ModelPort_GoldenReference_ThirdBatch_V41_ALL.zip"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "V4.1 finished" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green

if (Test-Path -LiteralPath $Summary -PathType Leaf) {
    Get-Content -LiteralPath $Summary
}

Write-Host ""
Write-Host "Evidence ZIP:" -ForegroundColor Yellow
Write-Host $Zip -ForegroundColor White