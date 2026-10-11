# Browser build (WOLF-41): cmake --preset web && cmake --build --preset web
# Makes raylib_wolf3d.html, .js, .wasm and, with the shareware data, .data.
# See "WebAssembly build" in README.rst for serving it.

# The shareware v1.4 files are bundled into the .data package from a local
# directory outside the repository (scripts/fetch_shareware.sh).  Without
# them the page still builds; it then tells the player the data is missing.
set(WOLF3D_WEB_DATA "$ENV{HOME}/wolf3d-data/shareware" CACHE PATH
  "Shareware WL1 data to bundle into the web build (empty: no data)")

# Same names and checksums as scripts/fetch_shareware.sh.  Never bundle
# registered data (*.WL6, *.SOD).
set(WOLF3D_WL1_SHA256
  AUDIOHED.WL1 39351624ae6f8eef4b873e060c1a6f3e5ee7e81c4939c485275a89b145336338
  AUDIOT.WL1   1e2c9ae30398a14c61a4ddd39aabaa0dbcc984cc4924a3e51df75574c259cfb9
  GAMEMAPS.WL1 a6a6654b342f2c027bcb22bfce0a41f9fc0063b775e9e4da0c771970e53e11aa
  MAPHEAD.WL1  3458f661c9b875bca99ea22a7267771fad8f2a33c699ea732cf7c3322909bf8c
  VGADICT.WL1  59878fec65f033b00dbb1240317d1793f5858213dc8013b4d68f9ac8b45b0c80
  VGAGRAPH.WL1 d5176f843c53415132db199c19f38591eaf3cedd35a4f7eb2698c1865d83030d
  VGAHEAD.WL1  f4cc800dc8444373092d4eaa5d6ab59d63d23a510a9d38b73ca9b3dbb700d18b
  VSWAP.WL1    698f217257e2cbb951a4d110ba09140291f38d0121b3784d1d6be59c03a6b47b)

set(WOLF3D_WEB_PRELOAD)
set(WOLF3D_WEB_DATA_FILES)
if(WOLF3D_WEB_DATA)
  set(missing)
  list(LENGTH WOLF3D_WL1_SHA256 n)
  math(EXPR last "${n} - 1")
  foreach(i RANGE 0 ${last} 2)
    math(EXPR j "${i} + 1")
    list(GET WOLF3D_WL1_SHA256 ${i} name)
    list(GET WOLF3D_WL1_SHA256 ${j} expected)
    set(file "${WOLF3D_WEB_DATA}/${name}")
    if(NOT EXISTS "${file}")
      list(APPEND missing ${name})
      continue()
    endif()
    file(SHA256 "${file}" hash)
    if(NOT hash STREQUAL expected)
      message(FATAL_ERROR "${file} is not the shareware v1.4 file (SHA-256 ${hash})")
    endif()
    list(APPEND WOLF3D_WEB_DATA_FILES "${file}")
    # SHELL: keeps CMake from de-duplicating the repeated option
    list(APPEND WOLF3D_WEB_PRELOAD "SHELL:--preload-file ${file}@/wolf3d/shareware/${name}")
  endforeach()
  if(missing)
    message(WARNING "No shareware data bundled: ${missing} not in ${WOLF3D_WEB_DATA}")
    set(WOLF3D_WEB_PRELOAD)
    set(WOLF3D_WEB_DATA_FILES)
  else()
    message(STATUS "Bundling shareware data from ${WOLF3D_WEB_DATA}")
  endif()
endif()

set(WOLF3D_SHELL ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)

# Hosting template (WOLF-43): the build directory is what gets uploaded.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/.htaccess
  ${CMAKE_CURRENT_BINARY_DIR}/.htaccess COPYONLY)

set_target_properties(raylib_wolf3d PROPERTIES
  SUFFIX .html
  LINK_DEPENDS "${WOLF3D_SHELL};${WOLF3D_WEB_DATA_FILES}")

# ASYNCIFY lets the game keep its nested blocking loops (menus, IN_Ack,
# fades, intermissions, demos): emscripten_sleep() in the frame and input
# pump unwinds to the browser and resumes there.  ASYNCIFY_STACK_SIZE holds
# the unwound call stack.  Indirect calls stay instrumented: menu items,
# PM_Preload's progress callback and actor think functions reach a yield
# through function pointers.  (ASYNCIFY_IGNORE_INDIRECT would save about
# 120 KB of the 150 KB ASYNCIFY adds to the .wasm, but a missed blocking
# path then traps at run time.)
set(WOLF3D_WEB_ASYNCIFY -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536)

target_link_options(raylib_wolf3d PRIVATE
  ${WOLF3D_WEB_ASYNCIFY}
  -sUSE_GLFW=3
  -sINITIAL_MEMORY=64MB
  -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=1MB
  -sENVIRONMENT=web
  # The page starts the game from a click (focus, pointer lock and, in a
  # later stage, the audio context need a user gesture).
  -sINVOKE_RUN=0
  # Quit and errors end the program; the page then shows its overlay.
  -sEXIT_RUNTIME=1
  -sEXPORTED_RUNTIME_METHODS=callMain,FS,ENV,addRunDependency,removeRunDependency
  # Config, saves and the player's own data files persist in IndexedDB
  # (web/shell.html mounts IDBFS; WL_UserFilesChanged syncs it).
  -lidbfs.js
  --shell-file ${WOLF3D_SHELL}
  ${WOLF3D_WEB_PRELOAD})
