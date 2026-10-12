#!/usr/bin/env bash
# Build the browser version from a clean checkout and publish it over SSH
# (WOLF-46).
#
#   scripts/deploy_web.sh [--ref REF] [--data DIR] [--host USER@HOST]
#                         [--root DIR] [--name NAME] [--dry-run]
#   scripts/deploy_web.sh --root-bypass [--host USER@HOST] [--root DIR]
#
# REF defaults to origin/master and is resolved to a commit SHA.  The build
# runs in a temporary `git worktree` of that SHA, so local changes never
# reach the server.  DIR is the shareware v1.4 data (default
# ~/wolf3d-data/shareware); every file is SHA-256 checked first.
#
# On the server (default ddscards@raydelto.org, ~/public_html/raydelto):
#   1. rsync the release to NAME.staging-<sha> (default NAME is wolf3d);
#   2. delete the old NAME.prev, rename NAME to NAME.prev, then rename the
#      staging directory to NAME.  The two renames run in one shell command,
#      so the site is without NAME for microseconds only.
# Rollback:  cd ~/public_html/raydelto && mv wolf3d wolf3d.bad && mv wolf3d.prev wolf3d
#
# --dry-run builds and assembles the release locally and prints where;
# nothing is uploaded.
#
# --root-bypass is a one-time step: the site's root .htaccess sends unknown
# paths to its React index.html.  It copies that file to
# .htaccess.bak-<UTC stamp> and adds `RewriteRule ^NAME/ - [L]` after the
# existing bypasses.  It changes nothing if the rule is already there.
#
# Needs: git, emsdk at $EMSDK or ~/emsdk, cmake, ninja, rsync, ssh, curl.
# Uses the caller's SSH key; the script holds no secrets.
set -euo pipefail

ref=origin/master
data="$HOME/wolf3d-data/shareware"
host=ddscards@raydelto.org
root=public_html/raydelto
name=wolf3d
url=https://www.raydelto.org
dry_run=0
root_bypass=0

usage() { sed -n '2,29p' "$0"; }
while [ $# -gt 0 ]; do
  case "$1" in
    --ref) ref="$2"; shift ;;
    --data) data="$2"; shift ;;
    --host) host="$2"; shift ;;
    --root) root="$2"; shift ;;
    --name) name="$2"; shift ;;
    --dry-run) dry_run=1 ;;
    --root-bypass) root_bypass=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done
case "$name" in
  ''|*[!A-Za-z0-9_-]*) echo "--name must be a plain directory name" >&2; exit 2 ;;
esac

ssh_opts=(-o BatchMode=yes -o ConnectTimeout=20)
remote() { ssh "${ssh_opts[@]}" "$host" "$@"; }

if [ "$root_bypass" = 1 ]; then
  # The remote script gets its values as arguments, never through quoting.
  remote bash -s -- "$root" "$name" <<'EOF'
set -euo pipefail
cd "$HOME/$1"
rule="RewriteRule ^$2/ - [L]"
if grep -qxF "$rule" .htaccess; then
  echo "already present: $rule"
  exit 0
fi
# Insert after the last existing bypass rule (^something/ - [L]).
last=$(grep -n '^RewriteRule \^[A-Za-z0-9_-]*/ - \[L\]$' .htaccess | tail -1 | cut -d: -f1)
if [ -z "$last" ]; then
  echo "no existing bypass rules in .htaccess; not editing it" >&2
  exit 1
fi
backup=".htaccess.bak-$(date -u +%Y%m%dT%H%M%SZ)"
cp -p .htaccess "$backup"
tmp=$(mktemp .htaccess.new.XXXXXX)
{
  head -n "$last" .htaccess
  printf '\n# Bypass all root-level rewrites for the /%s/ app\n%s\n' "$2" "$rule"
  tail -n +"$((last + 1))" .htaccess
} > "$tmp"
chmod --reference=.htaccess "$tmp"
mv "$tmp" .htaccess
echo "backup: $HOME/$1/$backup"
diff "$backup" .htaccess || true
EOF
  exit 0
fi

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
git -C "$repo" fetch --quiet origin
sha="$(git -C "$repo" rev-parse --verify "$ref^{commit}")"
short="${sha:0:12}"
echo "Deploying $ref = $sha"

# The data must be the shareware v1.4 set (the build checks it again).
"$repo/scripts/fetch_shareware.sh" --check-only "$data"

work="$(mktemp -d)"
cleanup() {
  git -C "$repo" worktree remove --force "$work/src" 2>/dev/null || true
  if [ "$dry_run" = 1 ]; then rm -rf "$work/src"; else rm -rf "$work"; fi
}
trap cleanup EXIT
git -C "$repo" worktree add --quiet --detach "$work/src" "$sha"

# shellcheck disable=SC1091
source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
emcc_version="$(emcc --version | head -1)"
echo "$emcc_version"
(
  cd "$work/src"
  cmake --preset web -DWOLF3D_WEB_DATA="$data" >/dev/null
  cmake --build --preset web
)

build="$work/src/build-web"
release="$work/release"
mkdir "$release"
cp "$build/raylib_wolf3d.html" "$release/index.html"
for f in raylib_wolf3d.js raylib_wolf3d.wasm raylib_wolf3d.data \
         wolf3d-wl6.js wolf3d-wl6.wasm variants.json .htaccess; do
  [ -f "$build/$f" ] && cp "$build/$f" "$release/$f"
done
for f in index.html raylib_wolf3d.js raylib_wolf3d.wasm raylib_wolf3d.data .htaccess; do
  [ -f "$release/$f" ] || { echo "build is missing $f" >&2; exit 1; }
done
{
  echo "commit: $sha"
  echo "built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "emcc: $emcc_version"
} > "$release/version.txt"
ls -l "$release"

if [ "$dry_run" = 1 ]; then
  echo "Dry run: release in $release (not uploaded)"
  exit 0
fi

staging="$name.staging-$short"
rsync -rlt --chmod=D755,F644 --delete -e "ssh ${ssh_opts[*]}" \
  "$release/" "$host:$root/$staging/"

remote bash -s -- "$root" "$name" "$staging" <<'EOF'
set -euo pipefail
cd "$HOME/$1"
name=$2 staging=$3
[ -f "$staging/index.html" ] || { echo "staging upload incomplete" >&2; exit 1; }
if [ -d "$name.prev" ]; then rm -rf -- "$name.prev"; fi
if [ -d "$name" ]; then
  mv -- "$name" "$name.prev" && mv -- "$staging" "$name"
else
  mv -- "$staging" "$name"
fi
cat "$name/version.txt"
EOF

echo "Live checks:"
base="$url/$name"
fail=0
check() {  # path expected-content-type (extended regex)
  local headers code type
  headers="$(curl -sSI --max-time 30 "$base/$1")"
  code="$(printf '%s' "$headers" | awk 'NR==1{print $2}')"
  type="$(printf '%s' "$headers" | tr -d '\r' | awk -F': ' 'tolower($1)=="content-type"{print $2}' | tail -1)"
  printf '  %-22s %s %s\n' "/$name/$1" "$code" "$type"
  if [ "$code" != 200 ] || ! printf '%s' "$type" | grep -Eq "^($2)"; then fail=1; fi
}
check "" text/html
check raylib_wolf3d.js "text/javascript|application/javascript"
check raylib_wolf3d.wasm application/wasm
check raylib_wolf3d.data application/octet-stream
if ! curl -fsS --max-time 30 "$base/version.txt" | grep -qx "commit: $sha"; then
  echo "  version.txt does not show $sha" >&2
  fail=1
fi
[ "$fail" = 0 ] || { echo "Live checks FAILED; rollback: ssh $host 'cd $root && mv $name $name.bad && mv $name.prev $name'" >&2; exit 1; }
echo "Deployed $sha to $base/"
