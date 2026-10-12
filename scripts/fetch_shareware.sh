#!/usr/bin/env bash
# Fetch the freely distributable Wolfenstein 3D v1.4 shareware data (*.WL1)
# into a cache outside the repository and verify every file by SHA-256.
#
#   scripts/fetch_shareware.sh [--check-only] [DIR]
#
# DIR defaults to ~/wolf3d-data/shareware.  Then run:
#   raylib_wolf3d -data DIR        (or WOLF3D_DATA=DIR raylib_wolf3d)
#
# Same archive and checksums as audio/tests/fetch_shareware.py (WOLF-3), which
# only extracts the audio files; this script extracts the full WL1 set.
# Never commit game data, and never use registered data (*.WL6, *.SOD).
set -euo pipefail

URL='https://beta.wolf3d.net/file-download/download/private/4457'
ARCHIVE_SHA256='3ff0798f7d912dcd03f78419b55d63dd1934a93b6b3be1fab425dc2b148612cc'
FILES=(
  'AUDIOHED.WL1 39351624ae6f8eef4b873e060c1a6f3e5ee7e81c4939c485275a89b145336338'
  'AUDIOT.WL1   1e2c9ae30398a14c61a4ddd39aabaa0dbcc984cc4924a3e51df75574c259cfb9'
  'GAMEMAPS.WL1 a6a6654b342f2c027bcb22bfce0a41f9fc0063b775e9e4da0c771970e53e11aa'
  'MAPHEAD.WL1  3458f661c9b875bca99ea22a7267771fad8f2a33c699ea732cf7c3322909bf8c'
  'VGADICT.WL1  59878fec65f033b00dbb1240317d1793f5858213dc8013b4d68f9ac8b45b0c80'
  'VGAGRAPH.WL1 d5176f843c53415132db199c19f38591eaf3cedd35a4f7eb2698c1865d83030d'
  'VGAHEAD.WL1  f4cc800dc8444373092d4eaa5d6ab59d63d23a510a9d38b73ca9b3dbb700d18b'
  'VSWAP.WL1    698f217257e2cbb951a4d110ba09140291f38d0121b3784d1d6be59c03a6b47b'
)

check_only=0
dest="$HOME/wolf3d-data/shareware"
for arg in "$@"; do
  case "$arg" in
    --check-only) check_only=1 ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    -*) echo "unknown option: $arg" >&2; exit 2 ;;
    *) dest="$arg" ;;
  esac
done

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
# realpath -m (GNU) also resolves a directory that does not exist yet; macOS's
# realpath has no -m, so create the directory first there.
if ! dest_abs="$(realpath -m "$dest" 2>/dev/null)"; then
  mkdir -p "$dest"
  dest_abs="$(cd "$dest" && pwd -P)"
fi
dest="$dest_abs"
case "$dest/" in
  "$repo"/*) echo "Keep game data outside the repository ($repo)" >&2; exit 1 ;;
esac
mkdir -p "$dest"

# sha256sum is missing before macOS 15; shasum ships with every macOS.
if command -v sha256sum >/dev/null; then
  sha() { sha256sum "$1" | cut -d' ' -f1; }
else
  sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
fi

if [ "$check_only" = 0 ]; then
  for tool in curl unzip; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
  done
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' EXIT
  curl -fsSL --max-time 120 -o "$tmp/shareware.zip" "$URL"
  if [ "$(sha "$tmp/shareware.zip")" != "$ARCHIVE_SHA256" ]; then
    echo "Archive SHA-256 mismatch; nothing written" >&2
    exit 1
  fi
  names=()
  for entry in "${FILES[@]}"; do names+=("${entry%% *}"); done
  mkdir "$tmp/x"
  unzip -q "$tmp/shareware.zip" "${names[@]}" -d "$tmp/x"
  for entry in "${FILES[@]}"; do
    read -r name expected <<<"$entry"
    if [ "$(sha "$tmp/x/$name")" != "$expected" ]; then
      echo "SHA-256 mismatch in archive: $name; nothing written" >&2
      exit 1
    fi
  done
  for name in "${names[@]}"; do cp "$tmp/x/$name" "$dest/$name"; done
fi

status=0
for entry in "${FILES[@]}"; do
  read -r name expected <<<"$entry"
  if [ -f "$dest/$name" ] && [ "$(sha "$dest/$name")" = "$expected" ]; then
    echo "PASS $name $expected"
  else
    echo "FAIL $name" >&2
    status=1
  fi
done
[ "$status" = 0 ] && echo "Shareware data ready in $dest"
exit "$status"
