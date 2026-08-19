param(
    [string]$BuildDir = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $Here "..\..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Repo "build-v46"
}

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter V4.6 Local Build + CTest" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Repo : $Repo" -ForegroundColor Cyan
Write-Host "Build: $BuildDir" -ForegroundColor Cyan
Write-Host ""

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Removing old build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

try {
    Get-Command cmake -ErrorAction Stop | Out-Null
} catch {
    throw "找不到 CMake，请先安装并加入 PATH。"
}

Write-Host "[1/3] Configure VS2022 x64..." -ForegroundColor Yellow
& cmake -S $Repo -B $BuildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }

Write-Host "[2/3] Build Debug..." -ForegroundColor Yellow
& cmake --build $BuildDir --config Debug --parallel 2
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }

Write-Host "[3/3] Run complete CTest..." -ForegroundColor Yellow
& ctest --test-dir $BuildDir -C Debug --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed: $LASTEXITCODE" }

$Converter = Join-Path $BuildDir "Debug\turtle335_convert_m2.exe"
$Probe = Join-Path $BuildDir "Debug\turtle335_probe_m2.exe"

Write-Host ""
Write-Host "PASS: V4.6 local build + CTest completed." -ForegroundColor Green
Write-Host "Whole-M2 converter:" -ForegroundColor Cyan
Write-Host "  $Converter" -ForegroundColor White
Write-Host "M2 probe:" -ForegroundColor Cyan
Write-Host "  $Probe" -ForegroundColor White
Write-Host ""
Write-Host "下一步可直接用真实 3.3.5a M2 + 同目录 skin/anim 跑 selected Golden。" -ForegroundColor Green
