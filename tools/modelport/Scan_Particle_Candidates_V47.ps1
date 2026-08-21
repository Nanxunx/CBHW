param(
    [string]$Root = "E:\335_FinalExtract_V5"
)

$Probe = ".\build-v46\Debug\turtle335_probe_m2.exe"
$Output = "golden\particle\candidates.txt"

if (!(Test-Path $Probe)) {
    throw "Missing probe executable: $Probe"
}

Remove-Item $Output -ErrorAction SilentlyContinue

$count = 0

Get-ChildItem $Root -Recurse -Filter *.m2 | ForEach-Object {

    $result = & $Probe $_.FullName 2>$null

    $particleLine = $result | Select-String "^particles="

    if ($particleLine) {

        $particles = [int](($particleLine.ToString()) -replace "particles=","")

        if ($particles -gt 0) {

            "$($_.FullName)|particles=$particles" |
                Add-Content $Output

            $count++

            Write-Host "FOUND $($_.Name) particles=$particles"
        }
    }
}

Write-Host ""
Write-Host "Completed."
Write-Host "Particle models found: $count"
Write-Host "Output: $Output"
