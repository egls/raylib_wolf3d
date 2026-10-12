# scripts/smoke_test.ps1 - Windows PowerShell no-data smoke tests for raylib_wolf3d
[CmdletBinding()]
param(
    [string]$GamePath = "build/raylib_wolf3d.exe"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $GamePath)) {
    Write-Error "Game executable not found at: $GamePath"
    exit 2
}

$GameFull = (Resolve-Path $GamePath).Path

Write-Host "Running smoke test on: $GameFull"

# Test 1: Missing data directory (must exit with code 2)
$missingDir = Join-Path $env:TEMP "wolf_smoke_nodata_$([guid]::NewGuid().ToString('N'))"
& $GameFull -data $missingDir 2>&1 | Out-Null
$rc = $LASTEXITCODE
if ($rc -ne 2) {
    Write-Error "Test 1 FAILED: expected exit code 2 on missing data directory, got $rc"
    exit 1
}
Write-Host "PASS: missing data directory reported with exit code 2"

# Test 2: Empty placeholder files (must exit with code 1 and report 'VSWAP: short read')
$fakeDir = Join-Path $env:TEMP "wolf_smoke_fake_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $fakeDir -Force | Out-Null
try {
    foreach ($f in "AUDIOHED", "AUDIOT", "GAMEMAPS", "MAPHEAD", "VGADICT", "VGAGRAPH", "VGAHEAD", "VSWAP") {
        New-Item -ItemType File -Path (Join-Path $fakeDir "$f.WL1") -Force | Out-Null
    }

    $out = & $GameFull -data $fakeDir 2>&1 | Out-String
    $rc = $LASTEXITCODE

    if ($rc -ne 1) {
        Write-Error "Test 2 FAILED: expected exit code 1 on empty placeholder data, got $rc.`nOutput:`n$out"
        exit 1
    }
    if ($out -notmatch "VSWAP:\s*short read") {
        Write-Error "Test 2 FAILED: expected 'VSWAP: short read' in output, got:`n$out"
        exit 1
    }
    Write-Host "PASS: placeholder files rejected with exit code 1 and 'VSWAP: short read'"
} finally {
    Remove-Item -Path $fakeDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "All Windows smoke tests PASSED."
exit 0
