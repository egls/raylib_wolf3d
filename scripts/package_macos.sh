#!/usr/bin/env bash
# Package a built raylib_wolf3d.app as a zip, without game data (macOS, WOLF-66).
#
#   scripts/package_macos.sh [-B BUILD_DIR] [-o OUT_DIR] VERSION
#
# BUILD_DIR defaults to build/, OUT_DIR to dist/.  Writes, for ARCH = `uname -m`:
#   OUT_DIR/raylib_wolf3d-VERSION-macos-ARCH.zip
#   OUT_DIR/SHA256SUMS-macos
# The zip holds raylib_wolf3d.app (ad-hoc signed; no Developer ID, not
# notarized), scripts/fetch_shareware.sh, scripts/macos_bundle_data.sh and
# doc/ (README, SOURCE.txt, licenses/).  An app with *.WL1 files inside is
# refused: the user fetches the shareware data.
set -euo pipefail
umask 022

build=build
out=dist
version=
while [ $# -gt 0 ]; do
  case "$1" in
    -B) build="$2"; shift 2 ;;
    -o) out="$2"; shift 2 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *) version="$1"; shift ;;
  esac
done
[ -n "$version" ] || { echo "usage: $0 [-B BUILD_DIR] [-o OUT_DIR] VERSION" >&2; exit 2; }
case "$version" in
  v*) echo "VERSION must not start with 'v' (got $version)" >&2; exit 2 ;;
esac
[ "$(uname -s)" = Darwin ] || { echo "macOS only" >&2; exit 1; }

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
build="$(cd "$build" && pwd -P)"
mkdir -p "$out"
out="$(cd "$out" && pwd -P)"
cd "$repo"
app="$build/raylib_wolf3d.app"
[ -x "$app/Contents/MacOS/raylib_wolf3d" ] || { echo "no app at $app; build first" >&2; exit 1; }

arch="$(uname -m)"
commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
min_os="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$app/Contents/Info.plist")"

raylib_license=
for f in "$build/_deps/raylib-src/LICENSE" /opt/homebrew/opt/raylib/LICENSE /usr/local/opt/raylib/LICENSE; do
  if [ -f "$f" ]; then raylib_license="$f"; break; fi
done
[ -n "$raylib_license" ] || { echo "raylib LICENSE not found (looked in $build/_deps/raylib-src)" >&2; exit 1; }

check_no_data () {
  local hits
  hits="$(find "$1" -type f \( -iname '*.wl1' -o -iname '*.wl6' -o -iname '*.sod' -o -iname '*.sdm' \
            -o -iname '*.wl3' -o -iname '*.sd1' -o -iname '*.sd2' -o -iname '*.sd3' \) -print)"
  if [ -n "$hits" ]; then echo "game data in package, refusing: $hits" >&2; exit 1; fi
}

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
name="raylib_wolf3d-$version-macos-$arch"
root="$tmp/$name"
mkdir -p "$root/scripts" "$root/doc/licenses"
ditto "$app" "$root/raylib_wolf3d.app"
check_no_data "$root"
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
cat > "$root/doc/SOURCE.txt" <<EOT
raylib_wolf3d $version (macOS $arch, macOS $min_os or later)
Source: https://github.com/raydelto/raylib_wolf3d
Commit: $commit

Rebuild (macOS, Xcode Command Line Tools + Homebrew cmake/ninja):
  git clone https://github.com/raydelto/raylib_wolf3d
  cd raylib_wolf3d && git checkout $commit
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build
EOT
check_no_data "$root"

zip="$out/$name.zip"
rm -f "$zip"
ditto -c -k --sequesterRsrc --keepParent "$root" "$zip"
(cd "$out" && shasum -a 256 "$name.zip" > SHA256SUMS-macos && cat SHA256SUMS-macos)
