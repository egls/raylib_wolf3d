#!/usr/bin/env bash
# Offline test of scripts/deploy_web_remote.sh and the deploy_web.sh
# dispatch (WOLF-46).  It runs the server side against a temporary $HOME,
# with stubs for ssh and for the renameat2 helper to inject failures.
# Needs Linux (renameat2) and python3.
#
#   tests/deploy_web_check.sh [REPO]
set -euo pipefail

repo="$(cd "${1:-$(dirname "$0")/..}" && pwd -P)"
remote_sh="$repo/scripts/deploy_web_remote.sh"
real_python="$(command -v python3)"
t="$(mktemp -d)"
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/home/site" "$t/bin"
site="$t/home/site"
printf 'RewriteRule ^rc/ - [L]\n\nRewriteRule ^nxr/ - [L]\n\nRewriteEngine On\n' \
  > "$site/.htaccess"

# python3 stub: FAIL_TO=PATH makes renames to PATH fail.
cat > "$t/bin/python3" <<EOF
#!/usr/bin/env bash
if [ "\${1:-}" = - ] && [ -n "\${FAIL_TO:-}" ] && [ "\${4:-}" = "\$FAIL_TO" ]; then
  cat >/dev/null; echo "injected failure: \$2 \$3 -> \$4" >&2; exit 1
fi
exec "$real_python" "\$@"
EOF
# ssh stub: drop options and host, run the command with the fake \$HOME.
cat > "$t/bin/ssh" <<EOF
#!/usr/bin/env bash
while [ "\$1" != bash ]; do shift; done
HOME="$t/home" exec "\$@"
EOF
chmod +x "$t/bin/python3" "$t/bin/ssh"
export PATH="$t/bin:$PATH"

pass=0
fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { pass=$((pass + 1)); echo "ok $pass - $*"; }
srv() { HOME="$t/home" bash "$remote_sh" "$1" site wolf3d "${@:2}"; }
live() { cat "$site/wolf3d/version.txt"; }
stage() {  # stage RELEASE: lock, upload, as deploy_web.sh does
  srv lock "wolf3d.staging-$1" >/dev/null
  echo "$1" > "$site/wolf3d.staging-$1/version.txt"
  touch "$site/wolf3d.staging-$1/index.html"
}

stage a; srv cutover wolf3d.staging-a >/dev/null
[ "$(live)" = a ] && [ ! -e "$site/wolf3d.prev" ] && [ ! -e "$site/wolf3d.deploy-lock" ] \
  || fail "first deploy"
ok "first deploy renames staging to wolf3d"

stage b; srv cutover wolf3d.staging-b >/dev/null
[ "$(live)" = b ] && [ "$(cat "$site/wolf3d.prev/version.txt")" = a ] || fail "second deploy"
ok "second deploy swaps, previous release is wolf3d.prev"

stage c; srv cutover wolf3d.staging-c >/dev/null
[ "$(live)" = c ] && [ "$(cat "$site/wolf3d.prev/version.txt")" = b ] \
  && [ -z "$(cd "$site" && ls -d wolf3d.staging-* 2>/dev/null)" ] || fail "third deploy"
ok "third deploy replaces the old wolf3d.prev"

stage d
if FAIL_TO=wolf3d srv cutover wolf3d.staging-d 2>/dev/null; then fail "exchange failure ignored"; fi
[ "$(live)" = c ] && [ "$(cat "$site/wolf3d.prev/version.txt")" = b ] \
  && [ -f "$site/wolf3d.staging-d/version.txt" ] && [ -d "$site/wolf3d.deploy-lock" ] \
  || fail "state after failed exchange"
ok "failed swap: live site, wolf3d.prev and staging unchanged, lock kept"

if srv lock wolf3d.staging-e 2>/dev/null; then fail "second lock taken"; fi
[ ! -e "$site/wolf3d.staging-e" ] || fail "staging made without the lock"
ok "a second deploy is refused while the lock is held"

srv unlock >/dev/null
[ ! -e "$site/wolf3d.staging-d" ] && [ ! -e "$site/wolf3d.deploy-lock" ] && [ "$(live)" = c ] \
  || fail "unlock"
ok "unlock removes the unused staging directory and the lock"

stage f
FAIL_TO=wolf3d.prev srv cutover wolf3d.staging-f >/dev/null 2>&1 || fail "housekeeping failure is fatal"
[ "$(live)" = f ] && [ ! -e "$site/wolf3d.deploy-lock" ] \
  && [ "$(cat "$site/wolf3d.prev/version.txt")" = b ] \
  && [ "$(cat "$site/wolf3d.staging-f/version.txt")" = c ] || fail "state after housekeeping failure"
ok "failed housekeeping after the swap: new release live, wolf3d.prev intact, lock released"
rm -rf "$site/wolf3d.staging-f"

srv rollback >/dev/null
[ "$(live)" = b ] && [ "$(cat "$site/wolf3d.prev/version.txt")" = f ] || fail "rollback"
srv rollback >/dev/null
[ "$(live)" = f ] || fail "roll forward"
ok "rollback swaps wolf3d and wolf3d.prev; again goes forward"

mv "$site/wolf3d" "$site/real"; ln -s real "$site/wolf3d"
if srv lock wolf3d.staging-g 2>/dev/null; then fail "symlink accepted"; fi
[ ! -e "$site/wolf3d.deploy-lock" ] || fail "lock left after refusal"
rm "$site/wolf3d"; mv "$site/real" "$site/wolf3d"
ok "a symlinked wolf3d is refused"

if srv lock other.staging-x 2>/dev/null; then fail "foreign staging name accepted"; fi
ok "staging names outside wolf3d.staging-* are refused"

before="$(sha256sum "$site/.htaccess")"
out="$(bash "$repo/scripts/deploy_web.sh" --root-bypass --dry-run --root site)"
[ "$(sha256sum "$site/.htaccess")" = "$before" ] \
  && [ -z "$(cd "$site" && ls -a | grep '^\.htaccess\.')" ] || fail "dry run changed .htaccess"
grep -qF '> RewriteRule ^wolf3d/ - [L]' <<<"$out" || fail "dry run shows no diff"
ok "--root-bypass --dry-run only prints the diff"

bash "$repo/scripts/deploy_web.sh" --root-bypass --root site >/dev/null
[ "$(grep -n 'wolf3d' "$site/.htaccess" | cut -d: -f1 | tail -1)" = 6 ] \
  && [ "$(ls "$site"/.htaccess.bak-* | wc -l)" = 1 ] || fail "bypass edit"
sed '4,6d' "$site/.htaccess" > "$t/rest"
cmp -s "$t/rest" "$(ls "$site"/.htaccess.bak-*)" || fail "bypass changed other lines"
out="$(bash "$repo/scripts/deploy_web.sh" --root-bypass --root site)"
grep -q 'already present' <<<"$out" && [ "$(ls "$site"/.htaccess.bak-* | wc -l)" = 1 ] \
  || fail "bypass not idempotent"
ok "--root-bypass backs up, adds only the rule after ^nxr/, and is idempotent"

echo "all $pass checks passed"
