<#
.SYNOPSIS
  Fetch the freely distributable Wolfenstein 3D v1.4 shareware data (*.WL1)
  into a cache outside the repository and verify every file by SHA-256.
.DESCRIPTION
  scripts\fetch_shareware.ps1 [-CheckOnly] [-Dir DIR]
  DIR defaults to $env:USERPROFILE\wolf3d-data\shareware.  Then run:
    raylib_wolf3d.exe -data DIR     (or $env:WOLF3D_DATA = DIR)
  Same archive and checksums as fetch_shareware.sh.  Never commit game data,
  and never use registered data (*.WL6, *.SOD).
#>
param(
  [switch]$CheckOnly,
  [string]$Dir = (Join-Path $env:USERPROFILE 'wolf3d-data\shareware')
)
$ErrorActionPreference = 'Stop'

$Url = 'https://beta.wolf3d.net/file-download/download/private/4457'
$ArchiveSha256 = '3ff0798f7d912dcd03f78419b55d63dd1934a93b6b3be1fab425dc2b148612cc'
$Files = [ordered]@{
  'AUDIOHED.WL1' = '39351624ae6f8eef4b873e060c1a6f3e5ee7e81c4939c485275a89b145336338'
  'AUDIOT.WL1'   = '1e2c9ae30398a14c61a4ddd39aabaa0dbcc984cc4924a3e51df75574c259cfb9'
  'GAMEMAPS.WL1' = 'a6a6654b342f2c027bcb22bfce0a41f9fc0063b775e9e4da0c771970e53e11aa'
  'MAPHEAD.WL1'  = '3458f661c9b875bca99ea22a7267771fad8f2a33c699ea732cf7c3322909bf8c'
  'VGADICT.WL1'  = '59878fec65f033b00dbb1240317d1793f5858213dc8013b4d68f9ac8b45b0c80'
  'VGAGRAPH.WL1' = 'd5176f843c53415132db199c19f38591eaf3cedd35a4f7eb2698c1865d83030d'
  'VGAHEAD.WL1'  = 'f4cc800dc8444373092d4eaa5d6ab59d63d23a510a9d38b73ca9b3dbb700d18b'
  'VSWAP.WL1'    = '698f217257e2cbb951a4d110ba09140291f38d0121b3784d1d6be59c03a6b47b'
}

function Get-Sha([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dest = [System.IO.Path]::GetFullPath($Dir)
if (Test-Path (Join-Path $repo '.git')) {
  if (($dest + '\').StartsWith($repo + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Keep game data outside the repository ($repo)"
  }
}
New-Item -ItemType Directory -Force -Path $dest | Out-Null

if (-not $CheckOnly) {
  $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("wolf3d-" + [guid]::NewGuid())
  New-Item -ItemType Directory -Path $tmp | Out-Null
  try {
    $zip = Join-Path $tmp 'shareware.zip'
    Invoke-WebRequest -Uri $Url -OutFile $zip -TimeoutSec 120
    if ((Get-Sha $zip) -ne $ArchiveSha256) { throw 'Archive SHA-256 mismatch; nothing written' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $x = Join-Path $tmp 'x'
    New-Item -ItemType Directory -Path $x | Out-Null
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
    try {
      foreach ($name in $Files.Keys) {
        $entry = $archive.Entries | Where-Object { $_.FullName -eq $name } | Select-Object -First 1
        if (-not $entry) { throw "$name not in archive; nothing written" }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $x $name))
      }
    } finally { $archive.Dispose() }
    foreach ($name in $Files.Keys) {
      if ((Get-Sha (Join-Path $x $name)) -ne $Files[$name]) { throw "SHA-256 mismatch in archive: $name; nothing written" }
    }
    foreach ($name in $Files.Keys) { Copy-Item -LiteralPath (Join-Path $x $name) -Destination (Join-Path $dest $name) -Force }
  } finally { Remove-Item -Recurse -Force -LiteralPath $tmp -ErrorAction SilentlyContinue }
}

$ok = $true
foreach ($name in $Files.Keys) {
  $p = Join-Path $dest $name
  if ((Test-Path -LiteralPath $p) -and ((Get-Sha $p) -eq $Files[$name])) { "PASS $name $($Files[$name])" }
  else { Write-Error "FAIL $name" -ErrorAction Continue; $ok = $false }
}
if ($ok) { "Shareware data ready in $dest"; exit 0 } else { exit 1 }
