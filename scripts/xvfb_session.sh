#!/bin/bash
# xvfb_session.sh - run raylib_wolf3d under a private Xvfb and drive it with
# scripts/xdrive.py.  Audio is irrelevant (silent build) but ALSA output is
# pointed at the null device anyway.
#
#   GAME=build/raylib_wolf3d DATA=~/wolf3d-data/shareware \
#     scripts/xvfb_session.sh "<game args>" xdrive-cmd...
#
# Exit code: the game's exit code (124 if it was still running at the end and
# had to be stopped).  Game output goes to ${LOG:-session.log}.
set -u
here=$(cd "$(dirname "$0")" && pwd)
GAME=${GAME:-build/raylib_wolf3d}
DATA=${DATA:-$HOME/wolf3d-data/shareware}
LOG=${LOG:-session.log}
gameargs=$1; shift

disp=:$((90 + RANDOM % 100))
Xvfb "$disp" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
xpid=$!
trap 'kill $xpid 2>/dev/null; wait $xpid 2>/dev/null' EXIT
for _ in $(seq 50); do [ -e "/tmp/.X11-unix/X${disp#:}" ] && break; sleep 0.1; done

export DISPLAY=$disp ALSA_CONFIG_PATH=${ALSA_CONFIG_PATH:-} AUDIODEV=null SDL_AUDIODRIVER=dummy
# shellcheck disable=SC2086
"$GAME" -data "$DATA" $gameargs >"$LOG" 2>&1 &
gpid=$!

python3 "$here/xdrive.py" "wait:15" "$@"
drc=$?

# give the game a moment to exit by itself (e.g. after a quit sequence)
for _ in $(seq ${EXIT_WAIT:-30}); do kill -0 $gpid 2>/dev/null || break; sleep 0.1; done
if kill -0 $gpid 2>/dev/null; then
	kill $gpid; wait $gpid 2>/dev/null; rc=124
else
	wait $gpid; rc=$?
fi
[ $drc -ne 0 ] && echo "xdrive failed ($drc)" >&2
exit $rc
