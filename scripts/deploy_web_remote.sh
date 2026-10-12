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
#                            release that was live as NAME.prev, release the
#                            lock; any failure switches back and keeps it
#   unlock ROOT NAME         after a failed upload or cutover: delete the
#                            unused STAGING and release the lock (refused
#                            while STAGING may hold a published release)
#   rollback ROOT NAME       atomically swap NAME and NAME.prev (run it
#                            again to go forward)
#   bypass ROOT NAME [--dry-run]
#                            add `RewriteRule ^NAME/ - [L]` to ROOT/.htaccess
#                            after a backup; --dry-run only prints the diff
#
# The swap is renameat2(RENAME_EXCHANGE) through python3: one system call,
# so NAME is never missing.  A cutover first moves NAME.prev aside to
# NAME.retired-<UTC>, swaps, then renames the old release to NAME.prev; if
# a step fails it undoes the earlier ones with the same renames.  The lock
# records the phase: "uploaded" (STAGING holds only the new upload, unlock
# may delete it) or "needs-recovery" (undoing failed too; unlock refuses and
# the state is fixed by hand).  The host (kernel 4.18, glibc 2.28, ext4) supports it; coreutils
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

# The lock directory records how far a deploy got.
phase() { cat "$lock/phase" 2>/dev/null || echo uploaded; }
set_phase() { echo "$1" > "$lock/phase"; }

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
    [ "$(phase)" = uploaded ] || {
      echo "$lock is in phase $(phase); see $PWD/$lock" >&2; exit 1; }
    [ -d "$staging" ] && [ ! -L "$staging" ] && [ -f "$staging/index.html" ] \
      && [ -f "$staging/version.txt" ] || {
      echo "staging upload incomplete: $PWD/$staging" >&2; exit 1; }
    plain_dir_or_absent "$name"
    plain_dir_or_absent "$name.prev"
    if [ ! -e "$name" ]; then
      rename2 noreplace "$staging" "$name"
      echo "live: $(head -1 "$name/version.txt")"
      rm -r "$lock"
      exit 0
    fi
    # Every step below is a rename.  Until the last one succeeds, a failure
    # puts everything back and keeps the lock in phase "uploaded" ($staging
    # holds the new release, so `unlock` may delete it).  If putting back
    # fails too, the phase is "needs-recovery" and `unlock` refuses.
    retired=
    if [ -e "$name.prev" ]; then
      retired="$name.retired-$(date -u +%Y%m%dT%H%M%SZ)"
      echo "$retired" > "$lock/retired"
      rename2 noreplace "$name.prev" "$retired"     # nothing changed if this fails
    fi
    restore_prev() {
      [ -z "$retired" ] || rename2 noreplace "$retired" "$name.prev"
    }
    give_up() {
      set_phase needs-recovery
      echo "$1; manual recovery needed, state:" >&2
      ls -d "$name" "$name".* >&2 || true
      echo "the lock $PWD/$lock stays until the state is fixed by hand" >&2
      exit 1
    }
    set_phase swapping
    if ! rename2 exchange "$staging" "$name"; then
      restore_prev || give_up "could not restore $name.prev from $retired"
      set_phase uploaded
      echo "switch failed; nothing changed, lock kept" >&2
      exit 1
    fi
    # $name is the new release, $staging the one that was live until now.
    if ! rename2 noreplace "$staging" "$name.prev"; then
      rename2 exchange "$staging" "$name" \
        || give_up "could not move the previous release to $name.prev or switch back"
      restore_prev || give_up "switched back, but could not restore $name.prev from $retired"
      set_phase uploaded
      echo "could not keep the previous release as $name.prev; switched back, lock kept" >&2
      exit 1
    fi
    echo "live: $(head -1 "$name/version.txt")"
    if [ -n "$retired" ] && ! rm -rf -- "$retired"; then
      echo "warning: could not delete $PWD/$retired" >&2
    fi
    rm -r "$lock"
    ;;

  unlock)
    if [ -d "$lock" ]; then
      if [ "$(phase)" != uploaded ]; then
        echo "$lock is in phase $(phase): $(cat "$lock/owner")" >&2
        echo "the staging directory may hold a published release; not unlocking" >&2
        exit 1
      fi
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
