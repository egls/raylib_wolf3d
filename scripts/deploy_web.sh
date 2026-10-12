#!/usr/bin/env bash
# Build the browser version from a clean checkout and publish it over SSH
# (WOLF-46).
#
#   scripts/deploy_web.sh [--ref REF] [--data DIR] [--host USER@HOST]
#                         [--root DIR] [--name NAME] [--dry-run]
#   scripts/deploy_web.sh --root-bypass [--dry-run] [--host USER@HOST] [--root DIR]
#   scripts/deploy_web.sh --rollback|--unlock [--host USER@HOST] [--root DIR]
#
# REF defaults to origin/master and is resolved to a commit SHA.  The build
# runs in a temporary `git worktree` of that SHA, so local changes never
# reach the server.  DIR is the shareware v1.4 data (default
# ~/wolf3d-data/shareware); every file is SHA-256 checked first.
#
# On the server (default ddscards@raydelto.org, ~/public_html/raydelto),
# through scripts/deploy_web_remote.sh:
#   1. take the lock NAME.deploy-lock (default NAME is wolf3d) and create a
#      new NAME.staging-<sha12>-<UTC stamp>; rsync the release into it;
#   2. swap staging and NAME in one renameat2(RENAME_EXCHANGE) call, so NAME
#      is never missing.  The release that was live becomes NAME.prev; the
#      older NAME.prev is moved aside first and deleted at the end.  If any
#      step fails, the server switches back to the state before the deploy.
# A failed upload removes the staging directory and the lock again.  A
# failed cutover switches back and keeps the lock; --unlock then deletes
# the unused staging directory (it refuses if the switch-back failed too).
#
# --rollback swaps NAME and NAME.prev atomically; run it again to go
# forward.  Manual fallback (not atomic):
#   cd ~/public_html/raydelto && mv wolf3d wolf3d.bad && mv wolf3d.prev wolf3d
#
# --dry-run builds and assembles the release locally and prints where;
# nothing is uploaded and nothing on the server changes.
#
# --root-bypass is a one-time step: the site's root .htaccess sends unknown
# paths to its React index.html.  It copies that file to
# .htaccess.bak-<UTC stamp> and adds `RewriteRule ^NAME/ - [L]` after the
# existing bypasses.  It changes nothing if the rule is already there.  With
# --dry-run it only prints the diff.
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
rollback=0
unlock_only=0

usage() { sed -n '2,40p' "$0"; }
while [ $# -gt 0 ]; do
  case "$1" in
    --ref) ref="$2"; shift ;;
    --data) data="$2"; shift ;;
    --host) host="$2"; shift ;;
    --root) root="$2"; shift ;;
    --name) name="$2"; shift ;;
    --dry-run) dry_run=1 ;;
    --root-bypass) root_bypass=1 ;;
    --rollback) rollback=1 ;;
    --unlock) unlock_only=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done
case "$name" in
  ''|*[!A-Za-z0-9_-]*) echo "--name must be a plain directory name" >&2; exit 2 ;;
esac

ssh_opts=(-o BatchMode=yes -o ConnectTimeout=20)
remote_script="$(dirname "$0")/deploy_web_remote.sh"
# remote COMMAND [ARG]: run deploy_web_remote.sh on the server.  Values are
# passed as arguments, never through shell quoting.
remote() {
  ssh "${ssh_opts[@]}" "$host" bash -s -- "$1" "$root" "$name" "${@:2}" \
    < "$remote_script"
}

# Checked before anything reaches the server.
if [ $((root_bypass + unlock_only + rollback)) -gt 1 ]; then
  echo "--root-bypass, --unlock and --rollback are separate steps" >&2; exit 2
fi
if [ "$dry_run" = 1 ] && [ $((unlock_only + rollback)) = 1 ]; then
  echo "--unlock and --rollback have no dry run" >&2; exit 2
fi

if [ "$root_bypass" = 1 ]; then
  if [ "$dry_run" = 1 ]; then remote bypass --dry-run; else remote bypass; fi
  exit 0
fi
if [ "$unlock_only" = 1 ]; then
  remote unlock
  exit 0
fi
if [ "$rollback" = 1 ]; then
  remote rollback
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

staging="$name.staging-$short-$(date -u +%Y%m%dT%H%M%SZ)"
remote lock "$staging"
locked=1
unlock() { [ "$locked" = 0 ] || remote unlock || true; }
trap 'unlock; cleanup' EXIT
rsync -rlt --chmod=D755,F644 -e "ssh ${ssh_opts[*]}" \
  "$release/" "$host:$root/$staging/"
# The lock is released by a successful cutover; after a failed one the
# server keeps it, so a stale state is never overwritten silently.
locked=0
if ! remote cutover "$staging"; then
  echo "Cutover failed; see above.  Discard the upload with: $0 --unlock" >&2
  exit 1
fi

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
[ "$fail" = 0 ] || { echo "Live checks FAILED; roll back with: $0 --rollback" >&2; exit 1; }
echo "Deployed $sha to $base/"
