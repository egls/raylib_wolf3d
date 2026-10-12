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

# python3 stub for the rename helper: FAIL_TO=GLOB fails renames whose
# target matches GLOB; FAIL_CALLS="N M" fails the Nth and Mth rename of one
# server command.
cat > "$t/bin/python3" <<EOF
#!/usr/bin/env bash
if [ "\${1:-}" = - ]; then
  n=\$(( \$(cat "$t/count") + 1 )); echo \$n > "$t/count"
  case " \${FAIL_CALLS:-} " in *" \$n "*) cat >/dev/null; echo "injected failure \$n" >&2; exit 1 ;; esac
  if [ -n "\${FAIL_TO:-}" ] && [[ "\$4" == \$FAIL_TO ]]; then
    cat >/dev/null; echo "injected failure: \$2 \$3 -> \$4" >&2; exit 1
  fi
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
srv() { echo 0 > "$t/count"; HOME="$t/home" bash "$remote_sh" "$1" site wolf3d "${@:2}"; }
ver() { cat "$site/$1/version.txt" 2>/dev/null || echo none; }
live() { ver wolf3d; }
prev() { ver wolf3d.prev; }
locked() { [ -d "$site/wolf3d.deploy-lock" ]; }
leftovers() { (cd "$site" && ls -d wolf3d.staging-* wolf3d.retired-* 2>/dev/null) || true; }
stage() {  # stage RELEASE: lock, upload, as deploy_web.sh does
  srv lock "wolf3d.staging-$1" >/dev/null
  echo "$1" > "$site/wolf3d.staging-$1/version.txt"
  touch "$site/wolf3d.staging-$1/index.html"
}
deploy() { stage "$1"; srv cutover "wolf3d.staging-$1" >/dev/null; }
# failed_cutover RELEASE: cutover must fail, switch back and keep the lock.
failed_cutover() {
  if srv cutover "wolf3d.staging-$1" >/dev/null 2>&1; then fail "cutover of $1 succeeded"; fi
  locked && [ "$(ver "wolf3d.staging-$1")" = "$1" ] || fail "staging/lock after failed cutover of $1"
}

deploy a
[ "$(live)" = a ] && [ "$(prev)" = none ] && ! locked || fail "first deploy"
ok "first deploy renames staging to wolf3d"

stage b; FAIL_TO=wolf3d.prev failed_cutover b
[ "$(live)" = a ] && [ "$(prev)" = none ] || fail "second deploy rotation failure"
if srv rollback 2>/dev/null; then fail "rollback ran while locked"; fi
srv unlock >/dev/null
[ "$(live)" = a ] && ! locked && [ -z "$(leftovers)" ] || fail "unlock after second deploy failure"
ok "second deploy: failing to keep a as wolf3d.prev switches back to a; unlock discards b"

deploy b
[ "$(live)" = b ] && [ "$(prev)" = a ] && ! locked || fail "second deploy"
deploy c
[ "$(live)" = c ] && [ "$(prev)" = b ] && [ -z "$(leftovers)" ] || fail "third deploy"
ok "later deploys keep the release that was live as wolf3d.prev, no leftovers"

stage d; FAIL_TO=wolf3d failed_cutover d
[ "$(live)" = c ] && [ "$(prev)" = b ] || fail "state after failed swap"
ok "failed swap: wolf3d and wolf3d.prev unchanged, lock kept"

if srv lock wolf3d.staging-e 2>/dev/null; then fail "second lock taken"; fi
[ ! -e "$site/wolf3d.staging-e" ] || fail "staging made without the lock"
ok "a second deploy is refused while the lock is held"

srv unlock >/dev/null
[ "$(live)" = c ] && ! locked && [ -z "$(leftovers)" ] || fail "unlock"
ok "unlock removes the unused staging directory and the lock"

stage f; FAIL_TO='wolf3d.retired-*' failed_cutover f
[ "$(live)" = c ] && [ "$(prev)" = b ] || fail "state after failed retire"
srv unlock >/dev/null
ok "failing to move the old wolf3d.prev aside changes nothing"

stage g; FAIL_CALLS=3 failed_cutover g   # 1 retire, 2 swap, 3 rotate
[ "$(live)" = c ] && [ "$(prev)" = b ] && [ -z "$(cd "$site" && ls -d wolf3d.retired-* 2>/dev/null)" ] \
  || fail "later deploy rotation failure"
srv unlock >/dev/null
srv rollback >/dev/null
[ "$(live)" = b ] && [ "$(prev)" = c ] || fail "rollback after rotation failure"
srv rollback >/dev/null
[ "$(live)" = c ] && [ "$(prev)" = b ] || fail "roll forward"
ok "later deploy: rotation failure switches back to c; rollback then returns to b, the release before c"

stage h
if FAIL_CALLS="3 4" srv cutover wolf3d.staging-h >/dev/null 2>&1; then fail "double failure succeeded"; fi
locked && [ "$(cat "$site/wolf3d.deploy-lock/phase")" = needs-recovery ] || fail "phase"
if srv unlock 2>/dev/null; then fail "unlock in needs-recovery"; fi
if srv rollback 2>/dev/null; then fail "rollback in needs-recovery"; fi
[ "$(live)" = h ] && [ "$(ver wolf3d.staging-h)" = c ] \
  && [ "$(ver "$(cd "$site" && ls -d wolf3d.retired-*)")" = b ] || fail "needs-recovery state"
ok "rotation and switch-back both failing: lock kept, unlock refuses to delete c"
# Put it back by hand, as the printed state allows.
(cd "$site" && rm -rf wolf3d && mv wolf3d.staging-h wolf3d && mv wolf3d.retired-* wolf3d.prev \
  && rm -r wolf3d.deploy-lock)
[ "$(live)" = c ] && [ "$(prev)" = b ] || fail "manual recovery"

stage i
if FAIL_CALLS="2 3" srv cutover wolf3d.staging-i >/dev/null 2>&1; then fail "swap+restore failure succeeded"; fi
[ "$(cat "$site/wolf3d.deploy-lock/phase")" = needs-recovery ] && [ "$(live)" = c ] \
  && [ "$(ver wolf3d.staging-i)" = i ] && [ "$(prev)" = none ] \
  && [ "$(ver "$(cd "$site" && ls -d wolf3d.retired-*)")" = b ] || fail "swap+restore failure state"
if bash "$repo/scripts/deploy_web.sh" --unlock --root site 2>/dev/null; then fail "--unlock in needs-recovery"; fi
ok "failed swap whose wolf3d.prev restore also fails: needs-recovery, nothing deleted"
(cd "$site" && mv wolf3d.retired-* wolf3d.prev && echo uploaded > wolf3d.deploy-lock/phase)
bash "$repo/scripts/deploy_web.sh" --unlock --root site >/dev/null
[ "$(live)" = c ] && [ "$(prev)" = b ] && ! locked && [ -z "$(leftovers)" ] || fail "--unlock"
ok "deploy_web.sh --unlock discards an unused upload"

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
