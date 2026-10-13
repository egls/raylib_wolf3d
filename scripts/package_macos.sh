#!/usr/bin/env bash
# Package a built raylib_wolf3d.app as a zip (macOS, WOLF-66, WOLF-111).
#
#   scripts/package_macos.sh [-B BUILD_DIR] [-o OUT_DIR] [--shareware DATA_DIR] VERSION
#
# BUILD_DIR defaults to build/, OUT_DIR to dist/.  Writes, for ARCH = `uname -m`:
#   OUT_DIR/raylib_wolf3d-VERSION-macos-ARCH[-shareware].zip
#   OUT_DIR/SHA256SUMS-macos
# The zip holds raylib_wolf3d.app (ad-hoc signed; no Developer ID, not
# notarized), scripts/fetch_shareware.sh, scripts/macos_bundle_data.sh and
# doc/ (README, SOURCE.txt, licenses/).  Without --shareware an app with
# *.WL1 files inside is refused: the user fetches the shareware data.  With
# --shareware the eight v1.4 *.WL1 files in DATA_DIR are checked against the
# SHA-256 sums in fetch_shareware.sh and copied into the app's
# Contents/Resources/ before it is signed; READ1ST.TXT and VENDOR.DOC from
# DATA_DIR, if present, go to doc/shareware/.  Registered data is refused.
set -euo pipefail
umask 022

build=build
out=dist
version=
shareware=
while [ $# -gt 0 ]; do
  case "$1" in
    -B) build="$2"; shift 2 ;;
    -o) out="$2"; shift 2 ;;
    --shareware) shareware="$2"; shift 2 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *) version="$1"; shift ;;
  esac
done
[ -n "$version" ] || { echo "usage: $0 [-B BUILD_DIR] [-o OUT_DIR] [--shareware DATA_DIR] VERSION" >&2; exit 2; }
case "$version" in
  v*) echo "VERSION must not start with 'v' (got $version)" >&2; exit 2 ;;
esac
[ "$(uname -s)" = Darwin ] || { echo "macOS only" >&2; exit 1; }

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
build="$(cd "$build" && pwd -P)"
mkdir -p "$out"
out="$(cd "$out" && pwd -P)"
if [ -n "$shareware" ]; then
  [ -d "$shareware" ] || { echo "no data directory: $shareware" >&2; exit 1; }
  shareware="$(cd "$shareware" && pwd -P)"
fi
cd "$repo"
app="$build/raylib_wolf3d.app"
[ -x "$app/Contents/MacOS/raylib_wolf3d" ] || { echo "no app at $app; build first" >&2; exit 1; }

# A distributable app may depend only on macOS-provided libraries. Shared
# raylib, Homebrew libraries and @rpath dependencies are not shipped here.
# Inspect load commands (rather than otool -L, whose names can contain spaces).
dependencies="$(otool -l "$app/Contents/MacOS/raylib_wolf3d" | awk '
  $1 == "cmd" { dylib = ($2 ~ /^LC_.*DYLIB$/ && $2 != "LC_ID_DYLIB") }
  dylib && $1 == "name" {
    sub(/^[[:space:]]*name /, ""); sub(/ \(offset [0-9]+\)$/, ""); print
  }
')"
while IFS= read -r dependency; do
  [ -n "$dependency" ] || continue
  case "$dependency" in
    /System/Library/*|/usr/lib/*) ;;
    *) echo "external runtime dependency: $dependency; rebuild with -DWOLF3D_USE_SYSTEM_RAYLIB=OFF -DBUILD_SHARED_LIBS=OFF" >&2
       exit 1 ;;
  esac
done <<< "$dependencies"

arch="$(uname -m)"
commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
min_os="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$app/Contents/Info.plist")"

raylib_license=
for f in "$build/_deps/raylib-src/LICENSE" /opt/homebrew/opt/raylib/LICENSE /usr/local/opt/raylib/LICENSE; do
  if [ -f "$f" ]; then raylib_license="$f"; break; fi
done
[ -n "$raylib_license" ] || { echo "raylib LICENSE not found (looked in $build/_deps/raylib-src)" >&2; exit 1; }

# check_no_data DIR [allow-wl1]: refuse game data; with allow-wl1 (the
# --shareware package) only *.WL1 files directly in the app's Resources/.
check_no_data () {
  local hits
  hits="$(find "$1" -type f \( -iname '*.wl1' -o -iname '*.wl6' -o -iname '*.sod' -o -iname '*.sdm' \
            -o -iname '*.wl3' -o -iname '*.sd1' -o -iname '*.sd2' -o -iname '*.sd3' \) -print)"
  [ "${2:-}" != allow-wl1 ] || hits="$(printf '%s\n' "$hits" | grep -v '/raylib_wolf3d\.app/Contents/Resources/[^/]*\.WL1$' || true)"
  if [ -n "$hits" ]; then echo "game data in package, refusing: $hits" >&2; exit 1; fi
}

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
name="raylib_wolf3d-$version-macos-$arch"
[ -z "$shareware" ] || name="$name-shareware"
root="$tmp/$name"
mkdir -p "$root/scripts" "$root/doc/licenses"
ditto "$app" "$root/raylib_wolf3d.app"
# The built app itself must not carry data: with --shareware exactly the
# verified files below go in.
check_no_data "$root"
if [ -n "$shareware" ]; then
  scripts/fetch_shareware.sh --check-only "$shareware"
  mkdir -p "$root/raylib_wolf3d.app/Contents/Resources"
  for f in AUDIOHED AUDIOT GAMEMAPS MAPHEAD VGADICT VGAGRAPH VGAHEAD VSWAP; do
    install -m 644 "$shareware/$f.WL1" "$root/raylib_wolf3d.app/Contents/Resources/$f.WL1"
  done
  scripts/fetch_shareware.sh --check-only "$root/raylib_wolf3d.app/Contents/Resources"
fi
# Version the copy, drop local symbols, then sign the bundle again.
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $version" \
  -c "Set :CFBundleVersion $version" "$root/raylib_wolf3d.app/Contents/Info.plist"
strip -x "$root/raylib_wolf3d.app/Contents/MacOS/raylib_wolf3d"
codesign --force --sign - "$root/raylib_wolf3d.app"
codesign --verify --strict --verbose=1 "$root/raylib_wolf3d.app"

install -m 755 scripts/fetch_shareware.sh scripts/macos_bundle_data.sh "$root/scripts/"
install -m 644 README.rst "$root/doc/README.rst"
install -m 644 WOLFSRC/README/LICENSE.DOC "$root/doc/licenses/id-software-LICENSE.DOC"
install -m 644 "$raylib_license" "$root/doc/licenses/raylib-LICENSE.zlib"
# The game always links the audio engine (CMake refuses WOLF3D_WITH_AUDIO=OFF).
install -m 644 audio/LICENSE "$root/doc/licenses/audio-LICENSE.GPL-2.0"
install -m 644 audio/vendor/LICENSE "$root/doc/licenses/nuked-opl3-LICENSE.LGPL-2.1"
if [ -n "$shareware" ]; then
  mkdir -p "$root/doc/shareware"
  for f in READ1ST.TXT VENDOR.DOC; do
    [ ! -f "$shareware/$f" ] || install -m 644 "$shareware/$f" "$root/doc/shareware/$f"
  done
  cat > "$root/doc/shareware/README.txt" <<'EOT'
raylib_wolf3d.app/Contents/Resources/ holds the original, unmodified id
Software Wolfenstein 3D v1.4 shareware data (*.WL1, episode 1), freely
distributable shareware.  Their SHA-256 checksums are pinned in
scripts/fetch_shareware.sh (check with: scripts/fetch_shareware.sh
--check-only raylib_wolf3d.app/Contents/Resources).  No registered or
commercial data is included.  Wolfenstein 3D is a trademark of its owner;
this project is not affiliated with id Software or ZeniMax.
EOT
fi
cat > "$root/doc/SOURCE.txt" <<EOT
raylib_wolf3d $version (macOS $arch, macOS $min_os or later)
Source: https://github.com/raydelto/raylib_wolf3d
Commit: $commit

Rebuild (macOS, Xcode Command Line Tools + Homebrew cmake/ninja):
  git clone https://github.com/raydelto/raylib_wolf3d
  cd raylib_wolf3d && git checkout $commit
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWOLF3D_USE_SYSTEM_RAYLIB=OFF -DBUILD_SHARED_LIBS=OFF && cmake --build build
EOT
check_no_data "$root" ${shareware:+allow-wl1}

zip="$out/$name.zip"
rm -f "$zip"
ditto -c -k --sequesterRsrc --keepParent "$root" "$zip"
(cd "$out" && shasum -a 256 "$name.zip" > SHA256SUMS-macos && cat SHA256SUMS-macos)
