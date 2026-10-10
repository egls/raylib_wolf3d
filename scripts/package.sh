#!/usr/bin/env bash
# Package a built raylib_wolf3d as a release tarball (and optionally a .deb).
#
#   scripts/package.sh [-B BUILD_DIR] [-o OUT_DIR] [--deb] VERSION
#
# BUILD_DIR defaults to build/, OUT_DIR to dist/.  VERSION is the release
# version without the leading "v" (e.g. 1.0.0, or 0.0.0~git1234567 for a dry
# run).  Writes, for ARCH = `uname -m`:
#   OUT_DIR/raylib_wolf3d-VERSION-linux-ARCH.tar.gz
#   OUT_DIR/raylib-wolf3d_VERSION_DEBARCH.deb          (with --deb)
#   OUT_DIR/SHA256SUMS
# No game data is ever packaged: the user fetches the shareware files with
# scripts/fetch_shareware.sh, which ships in both packages.
set -euo pipefail
shopt -s inherit_errexit
umask 022

build=build
out=dist
deb=0
version=
while [ $# -gt 0 ]; do
  case "$1" in
    -B) build="$2"; shift 2 ;;
    -o) out="$2"; shift 2 ;;
    --deb) deb=1; shift ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *) version="$1"; shift ;;
  esac
done
[ -n "$version" ] || { echo "usage: $0 [-B BUILD_DIR] [-o OUT_DIR] [--deb] VERSION" >&2; exit 2; }
case "$version" in
  v*) echo "VERSION must not start with 'v' (got $version)" >&2; exit 2 ;;
esac

build="$(realpath -m "$build")"
out="$(realpath -m "$out")"
repo="$(cd "$(dirname "$0")/.." && pwd -P)"
cd "$repo"
bin="$build/raylib_wolf3d"
[ -x "$bin" ] || { echo "no binary at $bin; build first" >&2; exit 1; }

arch="$(uname -m)"
commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
# Reproducible archive timestamps: the commit time, unless the caller set one.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || date +%s)}"

# raylib's license: from the FetchContent tree, or an installed raylib.
raylib_license=
for f in "$build/_deps/raylib-src/LICENSE" /usr/share/doc/raylib/LICENSE /usr/local/share/doc/raylib/LICENSE; do
  if [ -f "$f" ]; then raylib_license="$f"; break; fi
done
[ -n "$raylib_license" ] || { echo "raylib LICENSE not found (looked in $build/_deps/raylib-src)" >&2; exit 1; }

# The audio engine (GPL-2.0-or-later) and Nuked OPL3 (LGPL-2.1-or-later) are
# only listed when they are actually linked in.
with_audio=0
if nm "$bin" 2>/dev/null | grep -q ' T OPL3_'; then with_audio=1; fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$out"

# install_licenses DOC: README, license texts and SOURCE.txt into DOC.
install_licenses () {
  local doc="$1"
  install -d "$doc/licenses"
  install -m 644 README.rst "$doc/README.rst"
  install -m 644 WOLFSRC/README/LICENSE.DOC "$doc/licenses/id-software-LICENSE.DOC"
  install -m 644 "$raylib_license" "$doc/licenses/raylib-LICENSE.zlib"
  if [ "$with_audio" = 1 ]; then
    install -m 644 audio/LICENSE "$doc/licenses/audio-LICENSE.GPL-2.0"
    install -m 644 audio/vendor/LICENSE "$doc/licenses/nuked-opl3-LICENSE.LGPL-2.1"
  fi
  cat > "$doc/SOURCE.txt" <<EOF
raylib_wolf3d $version
Source: https://github.com/raydelto/raylib_wolf3d
Commit: $commit

Rebuild (Ubuntu 24.04):
  git clone https://github.com/raydelto/raylib_wolf3d
  cd raylib_wolf3d && git checkout $commit
  cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build
EOF
}

# Never ship game data, whatever ends up in the staging tree.
check_no_data () {
  local hits
  hits="$(find "$1" -type f \( -iname '*.wl1' -o -iname '*.wl6' -o -iname '*.sod' -o -iname '*.sdm' \
            -o -iname '*.wl3' -o -iname '*.sd1' -o -iname '*.sd2' -o -iname '*.sd3' \) -print)"
  if [ -n "$hits" ]; then echo "game data in package, refusing: $hits" >&2; exit 1; fi
}

tarball () {
  local name="raylib_wolf3d-$version-linux-$arch"
  local root="$tmp/tar/$name"
  install -d "$root/scripts"
  install -m 755 "$bin" "$root/raylib_wolf3d"
  strip --strip-unneeded "$root/raylib_wolf3d"
  install -m 755 scripts/fetch_shareware.sh "$root/scripts/fetch_shareware.sh"
  install_licenses "$root/doc"
  check_no_data "$root"
  tar -C "$tmp/tar" --sort=name --owner=0 --group=0 --numeric-owner \
      --mtime="@$SOURCE_DATE_EPOCH" -cf - "$name" | gzip -9n > "$out/$name.tar.gz"
  echo "$out/$name.tar.gz"
}

debian () {
  local pkg=raylib-wolf3d
  local maintainer="${DEB_MAINTAINER:-Raydelto Hernandez <raydelto@gmail.com>}"
  local debarch
  debarch="$(dpkg --print-architecture)"
  local root="$tmp/deb/root"
  local lib="$root/usr/lib/$pkg" share="$root/usr/share/$pkg" doc="$root/usr/share/doc/$pkg"
  install -d "$root/DEBIAN" "$root/usr/games" "$lib" "$share/scripts" "$doc"
  install -m 755 "$bin" "$lib/raylib_wolf3d"
  strip --strip-unneeded "$lib/raylib_wolf3d"
  install -m 755 scripts/fetch_shareware.sh "$share/scripts/fetch_shareware.sh"
  # The launcher only adds a default data directory; -data and WOLF3D_DATA
  # still take precedence.
  cat > "$root/usr/games/raylib_wolf3d" <<'EOF'
#!/bin/sh
: "${WOLF3D_DATA:=$HOME/wolf3d-data/shareware}"
export WOLF3D_DATA
exec /usr/lib/raylib-wolf3d/raylib_wolf3d "$@"
EOF
  chmod 755 "$root/usr/games/raylib_wolf3d"
  install_licenses "$doc"
  {
    echo "Wolfenstein 3D: Copyright (C) 1992 Id Software, Inc."
    echo "raylib: Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)"
    if [ "$with_audio" = 1 ]; then echo "Nuked OPL3: Copyright (C) 2013-2020 Nuke.YKT"; fi
    echo
    echo "This package contains no game data. Fetch the Wolfenstein 3D v1.4"
    echo "shareware files with /usr/share/$pkg/scripts/fetch_shareware.sh."
    echo
    echo "Licenses of the shipped code are in /usr/share/doc/$pkg/licenses/;"
    echo "see README.rst, section \"Licenses\"."
  } > "$doc/copyright"
  printf '%s (%s) unstable; urgency=low\n\n  * Release %s.\n  * Built from commit %s.\n\n -- %s  %s\n' \
    "$pkg" "$version" "$version" "$commit" "$maintainer" "$(date -R -u -d "@$SOURCE_DATE_EPOCH")" \
    | gzip -9n > "$doc/changelog.gz"
  check_no_data "$root"

  # libc/libm from the ELF; X11/GL are dlopen()ed by raylib's GLFW, and the
  # audio backends by miniaudio, so they are listed by hand.
  local shlibs
  install -d "$tmp/deb/src/debian"
  printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$pkg" "$pkg" > "$tmp/deb/src/debian/control"
  shlibs="$(cd "$tmp/deb/src" && dpkg-shlibdeps -O "$lib/raylib_wolf3d" 2>/dev/null | sed -n 's/^shlibs:Depends=//p')"
  local size
  size="$(du -sk --exclude=DEBIAN "$root" | cut -f1)"
  cat > "$root/DEBIAN/control" <<EOF
Package: $pkg
Version: $version
Architecture: $debarch
Maintainer: $maintainer
Installed-Size: $size
Depends: ${shlibs:+$shlibs, }libgl1, libx11-6, libxrandr2, libxinerama1, libxcursor1, libxi6, curl, unzip
Recommends: libasound2t64 | libasound2
Section: games
Priority: optional
Homepage: https://github.com/raydelto/raylib_wolf3d
Description: Wolfenstein 3D (1992) source port on raylib
 id Software's original Wolfenstein 3D engine ported to 64-bit C, with
 raylib as video, input and sound backend.  No game data is included:
 run /usr/share/$pkg/scripts/fetch_shareware.sh to download and
 verify the freely distributable v1.4 shareware episode.
EOF
  find "$root" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
  local file="$out/${pkg}_${version}_${debarch}.deb"
  dpkg-deb --root-owner-group -Zxz --build "$root" "$file" >/dev/null
  echo "$file"
}

files=("$(tarball)")
if [ "$deb" = 1 ]; then files+=("$(debian)"); fi
(cd "$out" && sha256sum "${files[@]##*/}" > SHA256SUMS && cat SHA256SUMS)
