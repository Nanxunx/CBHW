Write-Host "================================="
Write-Host " Turtle335Converter Project Scan"
Write-Host "================================="

Write-Host ""
Write-Host "[1] Source Files"
Write-Host "---------------------------------"

Get-ChildItem `
-src\m2 `
-Recurse `
-Filter *.cpp |
Select-Object FullName


Write-Host ""
Write-Host "[2] Header Files"
Write-Host "---------------------------------"

Get-ChildItem `
-Path include `
-Recurse `
-Filter *.h |
Select-Object FullName



Write-Host ""
Write-Host "[3] Particle Module"
Write-Host "---------------------------------"

Get-ChildItem `
-Path src\m2\particle `
-ErrorAction SilentlyContinue



Write-Host ""
Write-Host "[4] CMake Particle References"
Write-Host "---------------------------------"

Select-String `
-Path CMakeLists.txt `
-Pattern "particle|Particle"



Write-Host ""
Write-Host "[5] Git Status"
Write-Host "---------------------------------"

git status



Write-Host ""
Write-Host "================================="
Write-Host " Scan Complete"
Write-Host "================================="