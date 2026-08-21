$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=============================================="
Write-Host " Turtle335Converter Automated Verify"
Write-Host "=============================================="
Write-Host ""


Write-Host "[1/5] Git status"
git status


Write-Host ""
Write-Host "[2/5] CMake configure"

cmake -S . -B build-v46

if ($LASTEXITCODE -ne 0)
{
    throw "CMake configure failed"
}


Write-Host ""
Write-Host "[3/5] Build Debug"

cmake --build build-v46 --config Debug

if ($LASTEXITCODE -ne 0)
{
    throw "Build failed"
}


Write-Host ""
Write-Host "[4/5] Run CTest"

ctest --test-dir build-v46 -C Debug --output-on-failure

if ($LASTEXITCODE -ne 0)
{
    throw "CTest failed"
}


Write-Host ""
Write-Host "[5/5] Final repository check"

git status


Write-Host ""
Write-Host "=============================================="
Write-Host " SUCCESS"
Write-Host " Configure : PASS"
Write-Host " Build     : PASS"
Write-Host " Tests     : PASS"
Write-Host "=============================================="