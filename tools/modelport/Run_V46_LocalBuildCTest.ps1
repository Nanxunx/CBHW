param(
    [string]$BuildDir = "",
    [string]$EvidenceDir = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $Here "..\..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Repo "build-v46"
}
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $Repo "validation-v46"
}

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$RunDir = Join-Path $EvidenceDir ("V46_LocalValidation_" + $Stamp)
$ZipPath = $RunDir + ".zip"
$ConfigureLog = Join-Path $RunDir "01_configure.log"
$BuildLog = Join-Path $RunDir "02_build.log"
$CTestLog = Join-Path $RunDir "03_ctest.log"
$SummaryPath = Join-Path $RunDir "VALIDATION_SUMMARY.txt"

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

$ConfigureExit = $null
$BuildExit = $null
$CTestExit = $null
$Failure = ""
$Status = "FAIL"

function Format-ExitCode {
    param($Code)
    if ($null -eq $Code) { return "NOT_RUN" }
    return [string]$Code
}

function Get-CommandText {
    param([string]$Name, [string[]]$Args)
    try {
        $Text = (& $Name @Args 2>&1 | Out-String).Trim()
        if ([string]::IsNullOrWhiteSpace($Text)) { return "(no output)" }
        return $Text
    }
    catch {
        return "ERROR: $($_.Exception.Message)"
    }
}

function Get-GitHead {
    param([string]$Repository)
    try {
        Get-Command git -ErrorAction Stop | Out-Null
        $Head = (& git -C $Repository rev-parse HEAD 2>$null | Out-String).Trim()
        if ($LASTEXITCODE -eq 0 -and ![string]::IsNullOrWhiteSpace($Head)) {
            return $Head
        }
    }
    catch {}
    return "UNKNOWN"
}

function Get-Sha256Text {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path)) { return "MISSING" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter V4.6 Local Build + CTest" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Repo     : $Repo" -ForegroundColor Cyan
Write-Host "Build    : $BuildDir" -ForegroundColor Cyan
Write-Host "Evidence : $RunDir" -ForegroundColor Cyan
Write-Host ""

try {
    Get-Command cmake -ErrorAction Stop | Out-Null
    Get-Command ctest -ErrorAction Stop | Out-Null

    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "Removing old build directory..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }

    Write-Host "[1/3] Configure VS2022 x64..." -ForegroundColor Yellow
    & cmake -S $Repo -B $BuildDir -G "Visual Studio 17 2022" -A x64 2>&1 |
        Tee-Object -FilePath $ConfigureLog
    $ConfigureExit = $LASTEXITCODE
    if ($ConfigureExit -ne 0) {
        throw "CMake configure failed: $ConfigureExit"
    }

    Write-Host "[2/3] Build Debug..." -ForegroundColor Yellow
    & cmake --build $BuildDir --config Debug --parallel 2 2>&1 |
        Tee-Object -FilePath $BuildLog
    $BuildExit = $LASTEXITCODE
    if ($BuildExit -ne 0) {
        throw "CMake build failed: $BuildExit"
    }

    Write-Host "[3/3] Run complete CTest..." -ForegroundColor Yellow
    & ctest --test-dir $BuildDir -C Debug --output-on-failure 2>&1 |
        Tee-Object -FilePath $CTestLog
    $CTestExit = $LASTEXITCODE
    if ($CTestExit -ne 0) {
        throw "CTest failed: $CTestExit"
    }

    $Status = "PASS"
}
catch {
    $Failure = $_.Exception.Message
    Write-Host ""
    Write-Host ("FAIL: " + $Failure) -ForegroundColor Red
}
finally {
    $Converter = Join-Path $BuildDir "Debug\turtle335_convert_m2.exe"
    $Probe = Join-Path $BuildDir "Debug\turtle335_probe_m2.exe"

    $LastTest = Join-Path $BuildDir "Testing\Temporary\LastTest.log"
    if (Test-Path -LiteralPath $LastTest) {
        Copy-Item -LiteralPath $LastTest -Destination (Join-Path $RunDir "LastTest.log") -Force
    }

    $CMakeCache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $CMakeCache) {
        Copy-Item -LiteralPath $CMakeCache -Destination (Join-Path $RunDir "CMakeCache.txt") -Force
    }

    $GitHead = Get-GitHead $Repo
    $CMakeVersion = Get-CommandText "cmake" @("--version")
    $CTestVersion = Get-CommandText "ctest" @("--version")
    $ConverterSha = Get-Sha256Text $Converter
    $ProbeSha = Get-Sha256Text $Probe

    $ConfigureExitText = Format-ExitCode $ConfigureExit
    $BuildExitText = Format-ExitCode $BuildExit
    $CTestExitText = Format-ExitCode $CTestExit
    if ([string]::IsNullOrWhiteSpace($Failure)) { $Failure = "NONE" }

    $Summary = @"
Turtle335Converter V4.6 local validation evidence
================================================
GeneratedAt: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss K")
Status: $Status
GitHead: $GitHead
Repo: $Repo
BuildDir: $BuildDir

ExitCodes
---------
Configure: $ConfigureExitText
Build: $BuildExitText
CTest: $CTestExitText
Failure: $Failure

ToolVersions
------------
$CMakeVersion

$CTestVersion

Outputs
-------
Converter: $Converter
ConverterSHA256: $ConverterSha
Probe: $Probe
ProbeSHA256: $ProbeSha

EvidenceFiles
-------------
01_configure.log
02_build.log
03_ctest.log
LastTest.log (when CTest created it)
CMakeCache.txt (when configure created it)

Interpretation
--------------
PASS is valid only when Configure=0, Build=0, CTest=0 and Status=PASS.
A GitHub Actions zero-step failure is not a substitute for this local result.
"@

    Set-Content -LiteralPath $SummaryPath -Value $Summary -Encoding UTF8

    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }
    Compress-Archive -Path (Join-Path $RunDir "*") -DestinationPath $ZipPath -CompressionLevel Optimal -Force

    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host ("V4.6 validation status: " + $Status) -ForegroundColor $(if ($Status -eq "PASS") { "Green" } else { "Red" })
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "Summary:" -ForegroundColor Cyan
    Write-Host "  $SummaryPath" -ForegroundColor White
    Write-Host "Evidence ZIP:" -ForegroundColor Cyan
    Write-Host "  $ZipPath" -ForegroundColor White

    if ($Status -eq "PASS") {
        Write-Host "Whole-M2 converter:" -ForegroundColor Cyan
        Write-Host "  $Converter" -ForegroundColor White
        Write-Host "M2 probe:" -ForegroundColor Cyan
        Write-Host "  $Probe" -ForegroundColor White
        Write-Host ""
        Write-Host "PASS: real VS2022 build + complete CTest finished. Next gate is selected Golden regression." -ForegroundColor Green
    }
}

if ($Status -ne "PASS") {
    exit 1
}
