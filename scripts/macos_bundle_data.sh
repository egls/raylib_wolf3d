#!/usr/bin/env bash
# Copy verified shareware data into a locally built raylib_wolf3d.app, so the
# app runs from Finder without -data or WOLF3D_DATA (macOS, WOLF-66).
#
#   scripts/macos_bundle_data.sh [APP] [DATA_DIR]
#
# APP defaults to build/raylib_wolf3d.app, DATA_DIR to ~/wolf3d-data/shareware.
# The eight *.WL1 files are checked against the SHA-256 sums in
# fetch_shareware.sh first, copied to APP/Contents/Resources/ and the bundle
# is ad-hoc signed again.  For your own machine only: never redistribute an
# app with the data inside (README.rst, section "Releases").
set -euo pipefail

app="${1:-build/raylib_wolf3d.app}"
data="${2:-$HOME/wolf3d-data/shareware}"
case "$app" in -h|--help) sed -n '2,11p' "$0"; exit 0 ;; esac
repo="$(cd "$(dirname "$0")/.." && pwd -P)"

[ -f "$app/Contents/Info.plist" ] || { echo "not an app bundle: $app" >&2; exit 1; }
[ -d "$data" ] || { echo "no data directory: $data (run scripts/fetch_shareware.sh)" >&2; exit 1; }

"$repo/scripts/fetch_shareware.sh" --check-only "$data"
res="$app/Contents/Resources"
mkdir -p "$res"
for f in AUDIOHED AUDIOT GAMEMAPS MAPHEAD VGADICT VGAGRAPH VGAHEAD VSWAP; do
  cp "$data/$f.WL1" "$res/$f.WL1"
done
codesign --force --sign - "$app"
codesign --verify --strict "$app"
echo "Shareware data copied into $res; $app re-signed (ad-hoc)"
