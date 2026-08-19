param(
    [string]$SourceRoot = "E:\335_FinalExtract_V5",
    [string]$TargetRoot = "E:\335to112_Converted_FinalExtract_V1",
    [string]$AnimationData = "",
    [string]$V44Out = "E:\ModelPort_GoldenUpload_Targeted_V44",
    [string]$OutRoot = "E:\ModelPort_GoldenRegression_V46",
    [int]$StaticLimit = 3,
    [int]$AnimatedLimit = 4
)

$ErrorActionPreference = "Stop"

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = (Resolve-Path (Join-Path $Here "..\..")).Path
$Converter = Join-Path $Repo "build-v46\Debug\turtle335_convert_m2.exe"
$Scanner = Join-Path $Here "modelport_targeted_scan_v44.py"

Write-Host "============================================================" -ForegroundColor Green
Write-Host "Turtle335Converter V4.6 Selected Whole-M2 Golden Runner" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Source : $SourceRoot" -ForegroundColor Cyan
Write-Host "Target : $TargetRoot" -ForegroundColor Cyan
Write-Host "V44    : $V44Out" -ForegroundColor Cyan
Write-Host "Out    : $OutRoot" -ForegroundColor Cyan
Write-Host ""

if (!(Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "找不到 3.3.5a source root：$SourceRoot"
}
if (!(Test-Path -LiteralPath $TargetRoot -PathType Container)) {
    throw "找不到历史成功 1.12 target root：$TargetRoot"
}
if (!(Test-Path -LiteralPath $Converter -PathType Leaf)) {
    throw "找不到 V4.6 whole-M2 converter：$Converter。请先运行 Run_V46_LocalBuildCTest.ps1。"
}
if (!(Test-Path -LiteralPath $Scanner -PathType Leaf)) {
    throw "找不到 V4.4 targeted selector：$Scanner"
}

function Find-Python3 {
    foreach ($Candidate in @("py", "python", "python3")) {
        try {
            Get-Command $Candidate -ErrorAction Stop | Out-Null
            if ($Candidate -eq "py") {
                & $Candidate -3 --version *> $null
                if ($LASTEXITCODE -eq 0) { return @($Candidate, "-3") }
            } else {
                & $Candidate --version *> $null
                if ($LASTEXITCODE -eq 0) { return @($Candidate) }
            }
        } catch {}
    }
    return $null
}

$Meta = Join-Path $V44Out "STAGING\00_Metadata"
$HeaderCsv = Join-Path $Meta "V44_HeaderAndRiskIndex.csv"
$SelectedCsv = Join-Path $Meta "V44_SelectedSamples.csv"

if (!(Test-Path -LiteralPath $HeaderCsv -PathType Leaf) -or !(Test-Path -LiteralPath $SelectedCsv -PathType Leaf)) {
    Write-Host "V4.4 selected metadata 不存在，先运行 targeted selector（不是全库深扫）..." -ForegroundColor Yellow
    $Python = Find-Python3
    if ($null -eq $Python) { throw "找不到 Python 3" }

    if ($Python.Count -eq 2) {
        & $Python[0] $Python[1] $Scanner --source $SourceRoot --target $TargetRoot --out $V44Out --animation-regression-limit 24
    } else {
        & $Python[0] $Scanner --source $SourceRoot --target $TargetRoot --out $V44Out --animation-regression-limit 24
    }
    if ($LASTEXITCODE -ne 0) { throw "V4.4 targeted selector 失败，退出码：$LASTEXITCODE" }
}

if (!(Test-Path -LiteralPath $HeaderCsv -PathType Leaf) -or !(Test-Path -LiteralPath $SelectedCsv -PathType Leaf)) {
    throw "V4.4 targeted selector 未生成预期 metadata CSV"
}

# Build12340 AnimationData.dbc is required by the canonical V4.6 Playable table.
if ([string]::IsNullOrWhiteSpace($AnimationData)) {
    $Candidates = @(
        (Join-Path $SourceRoot "DBFilesClient\AnimationData.dbc"),
        (Join-Path $SourceRoot "dbc\AnimationData.dbc"),
        (Join-Path $SourceRoot "DBC\AnimationData.dbc"),
        (Join-Path $SourceRoot "AnimationData.dbc")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            $AnimationData = $Candidate
            break
        }
    }
    if ([string]::IsNullOrWhiteSpace($AnimationData)) {
        $Found = Get-ChildItem -LiteralPath $SourceRoot -Filter "AnimationData.dbc" -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $Found) { $AnimationData = $Found.FullName }
    }
}
if ([string]::IsNullOrWhiteSpace($AnimationData) -or !(Test-Path -LiteralPath $AnimationData -PathType Leaf)) {
    throw "找不到 Build12340 AnimationData.dbc。请用 -AnimationData 指定完整路径。"
}

Write-Host "AnimationData.dbc: $AnimationData" -ForegroundColor Cyan
Write-Host "Converter        : $Converter" -ForegroundColor Cyan
Write-Host ""

$HeaderRows = Import-Csv -LiteralPath $HeaderCsv
$SelectedRows = Import-Csv -LiteralPath $SelectedCsv
$script:Selection = @()
$script:Seen = @{}

function Add-Sample {
    param(
        [string]$Category,
        $Row
    )
    $Rel = [string]$Row.RelativePath
    if ([string]::IsNullOrWhiteSpace($Rel)) { return }
    $Key = $Rel.ToLowerInvariant()
    if ($script:Seen.ContainsKey($Key)) { return }
    $script:Seen[$Key] = $true
    $script:Selection += [pscustomobject]@{
        Category = $Category
        RelativePath = $Rel
        Animations = [int]($Row.Animations -as [int])
        Ribbons = [int]($Row.Ribbons -as [int])
        Particles = [int]($Row.Particles -as [int])
    }
}

$StaticRows = $HeaderRows | Where-Object {
    $_.Has112 -match '(?i)^(true|1|yes)$' -and
    [string]::IsNullOrWhiteSpace($_.HeaderError) -and
    $_.ConversionPlan -eq "STATIC_GEOMETRY_SAFE"
} | Sort-Object { [int]$_.Vertices } | Select-Object -First $StaticLimit

$AnimatedRows = $HeaderRows | Where-Object {
    $_.Has112 -match '(?i)^(true|1|yes)$' -and
    [string]::IsNullOrWhiteSpace($_.HeaderError) -and
    $_.ConversionPlan -eq "ANIMATION_BASELINE"
} | Sort-Object @{Expression={ [int]$_.Animations }; Descending=$true}, @{Expression={ [int]$_.Vertices }; Descending=$true} | Select-Object -First $AnimatedLimit

foreach ($Row in $StaticRows) { Add-Sample "00_StaticBaseline" $Row }
foreach ($Row in $AnimatedRows) { Add-Sample "00_AnimationBaseline" $Row }
foreach ($Row in $SelectedRows) { Add-Sample ([string]$Row.Category) $Row }

$Selection = $script:Selection
if ($Selection.Count -eq 0) { throw "没有找到可用于 V4.6 Golden regression 的 selected samples" }

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Evidence = Join-Path $OutRoot ("V46_SelectedGolden_" + $Stamp)
$GeneratedRoot = Join-Path $Evidence "Generated"
$GoldenRoot = Join-Path $Evidence "Historical112"
$SourceEvidenceRoot = Join-Path $Evidence "Source335"
$LogsRoot = Join-Path $Evidence "Logs"
$MetadataRoot = Join-Path $Evidence "Metadata"
New-Item -ItemType Directory -Force -Path $GeneratedRoot, $GoldenRoot, $SourceEvidenceRoot, $LogsRoot, $MetadataRoot | Out-Null

Copy-Item -LiteralPath $HeaderCsv -Destination (Join-Path $MetadataRoot "V44_HeaderAndRiskIndex.csv")
Copy-Item -LiteralPath $SelectedCsv -Destination (Join-Path $MetadataRoot "V44_SelectedSamples.csv")

$Results = @()
$Index = 0
foreach ($Sample in $Selection) {
    $Index++
    $Rel = [string]$Sample.RelativePath
    $Src = Join-Path $SourceRoot $Rel
    $Dst = Join-Path $TargetRoot $Rel
    $OutM2 = Join-Path $GeneratedRoot $Rel
    $Safe = ($Rel -replace '[\\/:*?"<>|]', '_')
    $Log = Join-Path $LogsRoot ("{0:D2}_{1}.log" -f $Index, $Safe)

    $Status = ""
    $ExitCode = -1
    $Message = ""

    Write-Host ("[{0}/{1}] {2}  {3}" -f $Index, $Selection.Count, $Sample.Category, $Rel) -ForegroundColor Yellow

    try {
        if (!(Test-Path -LiteralPath $Src -PathType Leaf)) { throw "source M2 missing" }
        if (!(Test-Path -LiteralPath $Dst -PathType Leaf)) { throw "historical target M2 missing" }

        $OutDir = Split-Path -Parent $OutM2
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

        $SourceCopy = Join-Path $SourceEvidenceRoot $Rel
        $SourceCopyDir = Split-Path -Parent $SourceCopy
        New-Item -ItemType Directory -Force -Path $SourceCopyDir | Out-Null
        Copy-Item -LiteralPath $Src -Destination $SourceCopy
        $SrcDir = Split-Path -Parent $Src
        $Stem = [IO.Path]::GetFileNameWithoutExtension($Src)
        Get-ChildItem -LiteralPath $SrcDir -File -ErrorAction SilentlyContinue | Where-Object {
            $_.BaseName.StartsWith($Stem, [System.StringComparison]::OrdinalIgnoreCase) -and
            ($_.Extension -ieq ".skin" -or $_.Extension -ieq ".anim")
        } | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $SourceCopyDir $_.Name)
        }

        $GoldenCopy = Join-Path $GoldenRoot $Rel
        $GoldenCopyDir = Split-Path -Parent $GoldenCopy
        New-Item -ItemType Directory -Force -Path $GoldenCopyDir | Out-Null
        Copy-Item -LiteralPath $Dst -Destination $GoldenCopy

        $PreviousEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $Converter $Src $AnimationData $OutM2 *> $Log
            $ExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $PreviousEap
        }
        $LogText = if (Test-Path -LiteralPath $Log) { Get-Content -LiteralPath $Log -Raw } else { "" }

        if ($ExitCode -eq 0 -and (Test-Path -LiteralPath $OutM2 -PathType Leaf)) {
            $Status = "GENERATED"
            Write-Host "  GENERATED" -ForegroundColor Green
        } elseif ($LogText -match "model contains Light records") {
            $Status = "SKIP_LIGHT_REFERENCE_GATED"
            $Message = "Light > 0 remains intentionally reference-gated"
            Write-Host "  SKIP Light reference-gated" -ForegroundColor DarkYellow
        } else {
            $Status = "CONVERTER_FAIL"
            $Message = ($LogText -replace "`r?`n", " | ").Trim()
            if ($Message.Length -gt 1000) { $Message = $Message.Substring($Message.Length - 1000) }
            Write-Host "  CONVERTER_FAIL" -ForegroundColor Red
        }
    } catch {
        $Status = "ERROR"
        $Message = $_.Exception.Message
        Set-Content -LiteralPath $Log -Value $Message -Encoding UTF8
        Write-Host ("  ERROR: " + $Message) -ForegroundColor Red
    }

    $GeneratedBytes = 0
    if (Test-Path -LiteralPath $OutM2 -PathType Leaf) {
        $GeneratedBytes = (Get-Item -LiteralPath $OutM2).Length
    }
    $GoldenBytes = 0
    if (Test-Path -LiteralPath $Dst -PathType Leaf) {
        $GoldenBytes = (Get-Item -LiteralPath $Dst).Length
    }

    $Results += [pscustomobject]@{
        Index = $Index
        Category = $Sample.Category
        RelativePath = $Rel
        Animations = $Sample.Animations
        Ribbons = $Sample.Ribbons
        Particles = $Sample.Particles
        Status = $Status
        ExitCode = $ExitCode
        GeneratedBytes = $GeneratedBytes
        Historical112Bytes = $GoldenBytes
        Message = $Message
    }
}

$Manifest = Join-Path $Evidence "V46_SelectedGolden_Manifest.csv"
$Results | Export-Csv -LiteralPath $Manifest -NoTypeInformation -Encoding UTF8

$Summary = [ordered]@{
    timestamp = $Stamp
    repository = $Repo
    branch = (git -C $Repo branch --show-current 2>$null)
    commit = (git -C $Repo rev-parse HEAD 2>$null)
    source_root = $SourceRoot
    target_root = $TargetRoot
    animation_data = $AnimationData
    animation_data_sha256 = (Get-FileHash -LiteralPath $AnimationData -Algorithm SHA256).Hash
    converter = $Converter
    converter_sha256 = (Get-FileHash -LiteralPath $Converter -Algorithm SHA256).Hash
    selected_total = $Results.Count
    generated = @($Results | Where-Object { $_.Status -eq "GENERATED" }).Count
    light_reference_gated = @($Results | Where-Object { $_.Status -eq "SKIP_LIGHT_REFERENCE_GATED" }).Count
    converter_fail = @($Results | Where-Object { $_.Status -eq "CONVERTER_FAIL" }).Count
    errors = @($Results | Where-Object { $_.Status -eq "ERROR" }).Count
    static_selected = @($Results | Where-Object { $_.Category -eq "00_StaticBaseline" }).Count
    animated_selected = @($Results | Where-Object { $_.Category -eq "00_AnimationBaseline" }).Count
    ribbon_selected = @($Results | Where-Object { [int]$_.Ribbons -gt 0 }).Count
    particle_selected = @($Results | Where-Object { [int]$_.Particles -gt 0 }).Count
    note = "This package is collection evidence. Semantic generated-vs-historical comparison is the next analysis step; historical Playable V4 bytes are not the V4.6 production oracle."
}

$SummaryJson = Join-Path $Evidence "V46_SelectedGolden_Summary.json"
$SummaryTxt = Join-Path $Evidence "V46_SelectedGolden_Summary.txt"
$Summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $SummaryJson -Encoding UTF8
$Summary.GetEnumerator() | ForEach-Object { "{0}: {1}" -f $_.Key, $_.Value } | Set-Content -LiteralPath $SummaryTxt -Encoding UTF8

$Zip = $Evidence + ".zip"
if (Test-Path -LiteralPath $Zip) { Remove-Item -LiteralPath $Zip -Force }
Compress-Archive -Path (Join-Path $Evidence "*") -DestinationPath $Zip -CompressionLevel Optimal

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "V4.6 selected whole-M2 evidence collection finished" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Evidence directory:" -ForegroundColor Cyan
Write-Host "  $Evidence" -ForegroundColor White
Write-Host "Evidence ZIP:" -ForegroundColor Cyan
Write-Host "  $Zip" -ForegroundColor White
Write-Host "Generated: $($Summary.generated) / $($Summary.selected_total)" -ForegroundColor Cyan
Write-Host "Light gated: $($Summary.light_reference_gated)" -ForegroundColor Cyan
Write-Host "Converter fail: $($Summary.converter_fail)" -ForegroundColor Cyan
Write-Host "Errors: $($Summary.errors)" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步：把这个 ZIP 上传到 ChatGPT，直接做 generated canonical v256 vs historical successful 1.12 semantic Golden comparison。" -ForegroundColor Green
