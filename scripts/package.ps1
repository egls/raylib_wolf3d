<#
.SYNOPSIS
  Package a built raylib_wolf3d.exe as a Windows release zip.
.DESCRIPTION
  scripts\package.ps1 [-BuildDir DIR] [-OutDir DIR] [-DllDir DIR[,DIR]] VERSION

  BuildDir defaults to build\ (a multi-config build keeps raylib_wolf3d.exe in
  a Release\ or RelWithDebInfo\ subdirectory; that is found too).  OutDir
  defaults to dist\.  VERSION has no leading "v".  Writes
    OutDir\raylib_wolf3d-VERSION-windows-x64.zip
    OutDir\SHA256SUMS-windows
  The zip holds the exe, every non-system runtime DLL it imports (found with
  objdump or dumpbin; searched for in DllDir, the exe's directory and PATH),
  scripts\fetch_shareware.ps1, a run.ps1 launcher and doc\ (README, SOURCE.txt,
  licenses\).  No game data is ever packaged.
#>
param(
  [string]$BuildDir = 'build',
  [string]$OutDir = 'dist',
  [string[]]$DllDir = @(),
  [Parameter(Mandatory = $true, Position = 0)][string]$Version
)
$ErrorActionPreference = 'Stop'
if ($Version.StartsWith('v')) { throw "Version must not start with 'v' (got $Version)" }

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = [System.IO.Path]::GetFullPath($BuildDir)
$out = [System.IO.Path]::GetFullPath($OutDir)
Set-Location $repo

$exe = $null
foreach ($c in 'raylib_wolf3d.exe', 'Release\raylib_wolf3d.exe', 'RelWithDebInfo\raylib_wolf3d.exe') {
  if (Test-Path -LiteralPath (Join-Path $build $c)) { $exe = Join-Path $build $c; break }
}
if (-not $exe) { throw "no raylib_wolf3d.exe in $build; build first" }

$commit = (& git rev-parse HEAD 2>$null); if ($LASTEXITCODE -ne 0) { $commit = 'unknown' }
$epoch = $env:SOURCE_DATE_EPOCH
if (-not $epoch) {
  $epoch = (& git log -1 --format=%ct 2>$null)
  if ($LASTEXITCODE -ne 0) { $epoch = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() }
}
$stamp = [DateTimeOffset]::FromUnixTimeSeconds([long]$epoch).UtcDateTime

# raylib's license: from the FetchContent tree or an installed raylib.
$raylibLicense = $null
foreach ($f in (Join-Path $build '_deps\raylib-src\LICENSE'),
               'C:\msys64\mingw64\share\licenses\raylib\LICENSE',
               'C:\msys64\mingw64\share\doc\raylib\LICENSE') {
  if (Test-Path -LiteralPath $f) { $raylibLicense = $f; break }
}
if (-not $raylibLicense) { throw "raylib LICENSE not found (looked in $build\_deps\raylib-src)" }

# ---- runtime DLLs ----------------------------------------------------------
function Get-Imports([string]$Path) {
  $objdump = Get-Command objdump -ErrorAction SilentlyContinue
  if ($objdump) {
    return (& $objdump.Source -p $Path) | ForEach-Object { if ($_ -match 'DLL Name:\s*(\S+)') { $Matches[1] } }
  }
  $dumpbin = Get-Command dumpbin -ErrorAction SilentlyContinue
  if ($dumpbin) {
    return (& $dumpbin.Source /dependents $Path) | ForEach-Object { if ($_ -match '^\s+(\S+\.dll)\s*$') { $Matches[1] } }
  }
  throw 'need objdump (MinGW) or dumpbin (MSVC Developer PowerShell) on PATH to list DLL imports'
}
$system = Join-Path $env:SystemRoot 'System32'
$searchDirs = @($DllDir) + @((Split-Path $exe)) + ($env:PATH -split ';' | Where-Object { $_ })
$dlls = @{}
$queue = [System.Collections.Generic.Queue[string]]::new()
$queue.Enqueue($exe)
while ($queue.Count -gt 0) {
  $cur = $queue.Dequeue()
  foreach ($dll in (Get-Imports $cur)) {
    if ($dlls.ContainsKey($dll.ToLowerInvariant())) { continue }
    # Windows itself and the API-set forwarders are never shipped.
    if ($dll -match '^(api-ms-win-|ext-ms-win-)') { continue }
    if (Test-Path -LiteralPath (Join-Path $system $dll)) { continue }
    $found = $null
    foreach ($d in $searchDirs) {
      if ($d -and (Test-Path -LiteralPath (Join-Path $d $dll))) { $found = Join-Path $d $dll; break }
    }
    if (-not $found) { throw "runtime DLL $dll not found (use -DllDir)" }
    $dlls[$dll.ToLowerInvariant()] = $found
    $queue.Enqueue($found)
  }
}

# ---- staging ---------------------------------------------------------------
$name = "raylib_wolf3d-$Version-windows-x64"
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("wolf3d-pkg-" + [guid]::NewGuid())
$root = Join-Path $tmp $name
$zipPath = Join-Path $out "$name.zip"
try {
  New-Item -ItemType Directory -Force -Path "$root\scripts", "$root\doc\licenses", $out | Out-Null
  Copy-Item -LiteralPath $exe -Destination "$root\raylib_wolf3d.exe"
  foreach ($p in $dlls.Values) { Copy-Item -LiteralPath $p -Destination $root }
  Copy-Item scripts\fetch_shareware.ps1 "$root\scripts\"
  Copy-Item README.rst "$root\doc\README.rst"
  Copy-Item WOLFSRC\README\LICENSE.DOC "$root\doc\licenses\id-software-LICENSE.DOC"
  Copy-Item -LiteralPath $raylibLicense "$root\doc\licenses\raylib-LICENSE.zlib"
  if (Test-Path audio\LICENSE) {
    Copy-Item audio\LICENSE "$root\doc\licenses\audio-LICENSE.GPL-2.0"
    Copy-Item audio\vendor\LICENSE "$root\doc\licenses\nuked-opl3-LICENSE.LGPL-2.1"
  }
  # Launcher: fetch the shareware data on first run, then start the game.
  @'
# Fetches the verified v1.4 shareware data on first run, then starts the game.
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs)
$data = if ($env:WOLF3D_DATA) { $env:WOLF3D_DATA } else { Join-Path $env:USERPROFILE 'wolf3d-data\shareware' }
if (-not (Test-Path (Join-Path $data 'VSWAP.WL1'))) {
  & "$PSScriptRoot\scripts\fetch_shareware.ps1" -Dir $data
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
& "$PSScriptRoot\raylib_wolf3d.exe" -data $data @GameArgs
exit $LASTEXITCODE
'@ | Set-Content -Encoding utf8 "$root\run.ps1"
  @"
raylib_wolf3d $Version (Windows x64)
Source: https://github.com/raydelto/raylib_wolf3d
Commit: $commit

Rebuild (Windows 11; MSYS2 MinGW-w64 or MSVC, see README.rst, "Windows"):
  git clone https://github.com/raydelto/raylib_wolf3d
  cd raylib_wolf3d; git checkout $commit
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo; cmake --build build
"@ | Set-Content -Encoding utf8 "$root\doc\SOURCE.txt"

  # Never ship game data, whatever ends up in the staging tree.
  $bad = Get-ChildItem -Recurse -File $root | Where-Object { $_.Extension -match '^\.(wl\d|sod|sdm|sd[123])$' }
  if ($bad) { throw "game data in package, refusing: $($bad.FullName -join ', ')" }

  # Reproducible zip: sorted entries, fixed timestamps, forward slashes.
  if (Test-Path $zipPath) { Remove-Item $zipPath }
  Add-Type -AssemblyName System.IO.Compression
  $fs = [System.IO.File]::Open($zipPath, 'Create')
  $zip = [System.IO.Compression.ZipArchive]::new($fs, [System.IO.Compression.ZipArchiveMode]::Create)
  try {
    $prefix = $tmp.TrimEnd('\') + '\'
    foreach ($f in (Get-ChildItem -Recurse -File $root | Sort-Object FullName)) {
      $e = $zip.CreateEntry($f.FullName.Substring($prefix.Length).Replace('\', '/'),
                            [System.IO.Compression.CompressionLevel]::Optimal)
      $e.LastWriteTime = [DateTimeOffset]$stamp
      $in = [System.IO.File]::OpenRead($f.FullName); $o = $e.Open()
      try { $in.CopyTo($o) } finally { $o.Dispose(); $in.Dispose() }
    }
  } finally { $zip.Dispose(); $fs.Dispose() }
} finally { Remove-Item -Recurse -Force -LiteralPath $tmp -ErrorAction SilentlyContinue }

$hash = (Get-FileHash -Algorithm SHA256 $zipPath).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content -Encoding ascii (Join-Path $out 'SHA256SUMS-windows')
"$hash  $name.zip"
'DLLs bundled: ' + $(if ($dlls.Count) { ($dlls.Keys | Sort-Object) -join ', ' } else { '(none)' })
