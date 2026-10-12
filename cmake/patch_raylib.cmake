# Applied by FetchContent (PATCH_COMMAND) to the raylib 5.5 source tree.
#
#   1. rcore.c: return from InitWindow() when InitPlatform() fails, so
#      IsWindowReady() is false and RL_OpenWindow() can report it.  raylib
#      5.5 called rlglInit() without a GL context and crashed (upstream fix).
#   2. rcore_desktop_glfw.c: do not ask a window that failed to open for its
#      position (glfwGetWindowPos(NULL) crashed before the error was seen).
#   3. GLFW nsgl_context.m: when macOS has no accelerated OpenGL pixel format
#      (a VM without a GPU, such as the GitHub macOS runners), retry without
#      NSOpenGLPFAAccelerated so the Apple software renderer is used.  Macs
#      with a GPU get the same pixel format as before.
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
"            glfwGetWindowPos(platform.handle, &wcx, &wcy);
"
"            if (platform.handle != NULL) glfwGetWindowPos(platform.handle, &wcx, &wcy);
")

wolf3d_patch(src/external/glfw/src/nsgl_context.m
"    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (window->context.nsgl.pixelFormat == nil)
"
"    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    // raylib_wolf3d: no GPU (e.g. a CI VM): fall back to the software renderer
    // by dropping NSOpenGLPFAAccelerated, the first attribute.
    if (window->context.nsgl.pixelFormat == nil)
    {
        window->context.nsgl.pixelFormat =
            [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs + 1];
    }
    if (window->context.nsgl.pixelFormat == nil)
")
