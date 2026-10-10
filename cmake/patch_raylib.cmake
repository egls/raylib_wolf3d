# Applied by FetchContent (PATCH_COMMAND) to the raylib 5.5 source tree.
#
# raylib 5.5 refuses to open a window when GLFW reports no monitor (an X
# server whose outputs are all disconnected, e.g. a headless GPU box), and
# InitWindow() then ignores that failure and calls rlglInit() with no GL
# context, which jumps through a NULL GL function pointer.
#   1. rcore.c: return from InitWindow() when InitPlatform() fails, so
#      IsWindowReady() is false and the caller can report it (upstream fix).
#   2. rcore_desktop_glfw.c: a windowed request with an explicit size does not
#      need a monitor; keep the requested size and skip centering.
# Each edit is idempotent and fails loudly if raylib's source changed.

function(wolf3d_patch file from to)
  file(READ "${file}" text)
  string(FIND "${text}" "${to}" done)
  if(NOT done EQUAL -1)
    return()
  endif()
  string(FIND "${text}" "${from}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "patch_raylib: expected text not found in ${file}")
  endif()
  string(REPLACE "${from}" "${to}" text "${text}")
  file(WRITE "${file}" "${text}")
endfunction()

wolf3d_patch(src/rcore.c
"    InitPlatform();
"
"    if (InitPlatform() != 0)
    {
        TRACELOG(LOG_WARNING, \"SYSTEM: Failed to initialize platform\");
        return;
    }
")

wolf3d_patch(src/platforms/rcore_desktop_glfw.c
"            // The monitor for the window-manager-created window can not be determined, so it can not be centered.
            glfwTerminate();
            TRACELOG(LOG_WARNING, \"GLFW: Failed to determine Monitor to center Window\");
            return -1;
"
"            // The monitor for the window-manager-created window can not be determined, so it can not be centered.
            // raylib_wolf3d: a window of explicit size works without one (headless X server).
            TRACELOG(LOG_WARNING, \"GLFW: Failed to determine Monitor to center Window\");
            if (requestWindowedFullscreen) { glfwTerminate(); return -1; }
")

wolf3d_patch(src/platforms/rcore_desktop_glfw.c
"        glfwGetMonitorWorkarea(monitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);
"
"        if (monitor != NULL) glfwGetMonitorWorkarea(monitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);
")
