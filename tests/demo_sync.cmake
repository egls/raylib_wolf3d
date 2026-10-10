# demo_sync.cmake - CTest driver for "raylib_wolf3d -demotest all" (WOLF-7).
#
# Plays the four shareware demos at full speed under Xvfb and compares the
# final state of each with the values recorded below.  The id demos were
# recorded until the player died, so a demo in sync plays every one of its
# input frames ("left=0") and ends with "end=died".
#
# The demos only stay in sync at view size 19 (full width): GunAttack picks
# targets within viewwidth/10 pixels of the centre, and at the default size
# 15 demo 0 hits a different guard at frame 956 and dies 97 frames early.
#
#   cmake -DGAME=<exe> -DDATA=<WL1 dir> -DWORK=<scratch dir> -P demo_sync.cmake
#
# Prints SKIPPED (see SKIP_REGULAR_EXPRESSION) without data or xvfb-run.

set(expected
  "demotest 0: map=0 frames=1152 left=0 tics=4608 end=died hash=f1e6cd20"
  "demotest 1: map=2 frames=1284 left=0 tics=5136 end=died hash=dec12253"
  "demotest 2: map=4 frames=671 left=0 tics=2684 end=died hash=47743191"
  "demotest 3: map=6 frames=633 left=0 tics=2532 end=died hash=4b0dd2a9")

if(NOT EXISTS "${DATA}/VSWAP.WL1")
  message("SKIPPED: no shareware data in ${DATA}")
  return()
endif()
find_program(XVFB_RUN xvfb-run)
if(NOT XVFB_RUN)
  message("SKIPPED: xvfb-run not found")
  return()
endif()

# a scratch config dir, so the user's CONFIG.WL1 can't change the result
file(MAKE_DIRECTORY "${WORK}")
set(ENV{XDG_CONFIG_HOME} "${WORK}")
set(ENV{ALSA_CONFIG_PATH} "")

execute_process(
  COMMAND ${XVFB_RUN} -a -s "-screen 0 640x480x24"
          ${GAME} -data ${DATA} -demotest all -viewsize 19
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc
  TIMEOUT 240)
message("${out}")

set(failed 0)
foreach(line IN LISTS expected)
  string(FIND "${out}" "${line} " pos)
  if(pos EQUAL -1)
    message("MISSING: ${line}")
    set(failed 1)
  endif()
endforeach()
if(NOT rc EQUAL 0 OR failed)
  message("${err}")
  message(FATAL_ERROR "demo_sync failed (exit ${rc})")
endif()
message("demo_sync: all 4 demos in sync")
