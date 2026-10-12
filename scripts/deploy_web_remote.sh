#!/usr/bin/env bash
# Server side of scripts/deploy_web.sh (WOLF-46).  deploy_web.sh pipes this
# file to `ssh HOST bash -s -- COMMAND ROOT NAME [ARG]`; ROOT is relative to
# the server's $HOME.  It only touches ROOT/NAME* and, for `bypass`, the
# single rule it adds to ROOT/.htaccess.
#
#   lock ROOT NAME STAGING   take NAME.deploy-lock, create the empty STAGING
#   cutover ROOT NAME STAGING
#                            atomically swap STAGING and NAME (or rename
#                            STAGING to NAME on the first deploy), keep the
#                            old release as NAME.prev, release the lock
#   unlock ROOT NAME         release the lock after a failed upload
#   rollback ROOT NAME       atomically swap NAME and NAME.prev (run it
#                            again to go forward)
#   bypass ROOT NAME [--dry-run]
#                            add `RewriteRule ^NAME/ - [L]` to ROOT/.htaccess
#                            after a backup; --dry-run only prints the diff
#
# The swap is renameat2(RENAME_EXCHANGE) through python3: one system call,
# so NAME is never missing and a failure leaves both directories as they
# were.  The host (kernel 4.18, glibc 2.28, ext4) supports it; coreutils
# 8.30 there has no `mv --exchange`.
set -euo pipefail

cmd=$1 root=$2 name=$3
case "$name" in
  ''|*[!A-Za-z0-9_-]*) echo "bad NAME: $name" >&2; exit 2 ;;
esac
cd "$HOME/$root"
lock="$name.deploy-lock"

# rename2 exchange|noreplace FROM TO
rename2() {
  python3 - "$@" <<'PY'
import ctypes, os, sys
flags = {"noreplace": 1, "exchange": 2}[sys.argv[1]]
libc = ctypes.CDLL(None, use_errno=True)
AT_FDCWD = -100
if libc.renameat2(AT_FDCWD, sys.argv[2].encode(), AT_FDCWD,
                  sys.argv[3].encode(), flags) != 0:
    sys.exit("renameat2 %s %s -> %s: %s" % (sys.argv[1], sys.argv[2],
             sys.argv[3], os.strerror(ctypes.get_errno())))
PY
}

# A path the deploy may replace must be absent or a plain directory.
plain_dir_or_absent() {
  if [ -L "$1" ] || { [ -e "$1" ] && [ ! -d "$1" ]; }; then
    echo "$PWD/$1 is not a plain directory; stopping" >&2
    exit 1
  fi
}

check_staging() {
  case "$1" in
    "$name".staging-*) ;;
    *) echo "bad STAGING: $1" >&2; exit 2 ;;
  esac
}

case "$cmd" in
  lock)
    staging=$4
    check_staging "$staging"
    plain_dir_or_absent "$name"
    plain_dir_or_absent "$name.prev"
    if ! mkdir "$lock" 2>/dev/null; then
      echo "another deploy holds $PWD/$lock:" >&2
      cat "$lock/owner" >&2 2>/dev/null || true
      echo "if it is stale, remove it with: rm -r $PWD/$lock" >&2
      exit 1
    fi
    echo "$staging $(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$lock/owner"
    if ! mkdir "$staging"; then
      rm -r "$lock"
      exit 1
    fi
    ;;

  cutover)
    staging=$4
    check_staging "$staging"
    [ -d "$lock" ] && grep -q "^$staging " "$lock/owner" || {
      echo "$lock is not held for $staging" >&2; exit 1; }
    [ -d "$staging" ] && [ ! -L "$staging" ] && [ -f "$staging/index.html" ] \
      && [ -f "$staging/version.txt" ] || {
      echo "staging upload incomplete: $PWD/$staging" >&2; exit 1; }
    plain_dir_or_absent "$name"
    plain_dir_or_absent "$name.prev"
    if [ -e "$name" ]; then
      # The switch.  After it, $staging holds the previous release.
      rename2 exchange "$staging" "$name"
      echo "live: $(head -1 "$name/version.txt")"
      # Housekeeping: the previous release becomes $name.prev, swapped in
      # too, so $name.prev is never missing.  The new release is already
      # live, so a failure here only warns.
      if [ -e "$name.prev" ]; then
        housekeeping() { rename2 exchange "$staging" "$name.prev" && rm -rf -- "$staging"; }
      else
        housekeeping() { rename2 noreplace "$staging" "$name.prev"; }
      fi
      if ! housekeeping; then
        echo "warning: previous release left in $PWD/$staging, not $name.prev" >&2
      fi
    else
      rename2 noreplace "$staging" "$name"
      echo "live: $(head -1 "$name/version.txt")"
    fi
    rm -r "$lock"
    ;;

  unlock)
    if [ -d "$lock" ]; then
      staging=$(cut -d' ' -f1 "$lock/owner")
      check_staging "$staging"
      echo "removing unused $staging and $lock"
      rm -rf -- "$staging"
      rm -r "$lock"
    fi
    ;;

  rollback)
    plain_dir_or_absent "$name"
    [ -d "$name" ] && [ -d "$name.prev" ] || {
      echo "need both $name and $name.prev" >&2; exit 1; }
    if ! mkdir "$lock" 2>/dev/null; then
      echo "a deploy holds $PWD/$lock; not rolling back" >&2; exit 1
    fi
    trap 'rm -r "$lock"' EXIT
    plain_dir_or_absent "$name.prev"
    rename2 exchange "$name.prev" "$name"
    echo "live: $(head -1 "$name/version.txt" 2>/dev/null || echo '?')"
    ;;

  bypass)
    dry=${4:-}
    rule="RewriteRule ^$name/ - [L]"
    if grep -qxF "$rule" .htaccess; then
      echo "already present: $rule"
      exit 0
    fi
    # Insert after the last existing bypass rule (^something/ - [L]).
    last=$(grep -n '^RewriteRule \^[A-Za-z0-9_-]*/ - \[L\]$' .htaccess \
           | tail -1 | cut -d: -f1)
    if [ -z "$last" ]; then
      echo "no existing bypass rules in .htaccess; not editing it" >&2
      exit 1
    fi
    edited() {
      head -n "$last" .htaccess
      printf '\n# Bypass all root-level rewrites for the /%s/ app\n%s\n' \
        "$name" "$rule"
      tail -n +"$((last + 1))" .htaccess
    }
    if [ "$dry" = --dry-run ]; then
      echo "dry run, .htaccess not changed; the edit would be:"
      edited | diff .htaccess - || true
      exit 0
    fi
    backup=".htaccess.bak-$(date -u +%Y%m%dT%H%M%SZ)"
    [ ! -e "$backup" ] || { echo "$backup exists" >&2; exit 1; }
    cp -p .htaccess "$backup"
    tmp=$(mktemp .htaccess.new.XXXXXX)
    edited > "$tmp"
    chmod --reference=.htaccess "$tmp"
    mv -- "$tmp" .htaccess
    echo "backup: $PWD/$backup"
    diff "$backup" .htaccess || true
    ;;

  *) echo "unknown command: $cmd" >&2; exit 2 ;;
esac
