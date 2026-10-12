# Headless core tests for the browser build (WOLF-98): the same test sources
# as the native suite, compiled to wasm32 by emcc and run under node by ctest
# (the Emscripten toolchain sets CMAKE_CROSSCOMPILING_EMULATOR to node).
# Only tests that need neither a display, threads, fork() nor Python driving
# the game binary are listed here; the browser checks are scripts/web_*.mjs.
#   cmake --preset web && cmake --build --preset web && ctest --preset web
option(WOLF3D_WEB_TESTS "Build the node-run core tests in the web build" ON)
if(NOT WOLF3D_WEB_TESTS)
  return()
endif()
enable_testing()

# NODERAWFS: core_wl1_data and sound_manager read the WL1 files straight
# from the host file system (they skip with 77 without them).
set(WOLF3D_WEB_TEST_LINK -sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1
  -sSTACK_SIZE=1MB -sEXIT_RUNTIME=1)
set(WOLF3D_WEB_TEST_DATA "$ENV{HOME}/wolf3d-data/shareware" CACHE PATH
  "External WL1 data for the node-run core tests")

add_executable(wolf_codec_test tests/codec_test.c port/wl_data.c)
target_include_directories(wolf_codec_test PRIVATE port)
target_compile_options(wolf_codec_test PRIVATE ${WOLF3D_TEST_WARNINGS})
add_test(NAME core_codecs COMMAND wolf_codec_test)

add_executable(wolf_render_test tests/render_test.c WOLFSRC/WL_DRAW.C WOLFSRC/WL_STATE.C
  WOLFSRC/WL_SCALE.C WOLFSRC/WOLFHACK.C WOLFSRC/ID_MM.C port/wl_raycast.c port/wl_data.c port/asm_stubs.c)
target_include_directories(wolf_render_test PRIVATE port WOLFSRC)
target_compile_options(wolf_render_test PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS} ${WOLF3D_SECTION_FLAGS})
target_link_libraries(wolf_render_test PRIVATE ${WOLF3D_LIBM})
add_test(NAME core_renderer COMMAND wolf_render_test)

add_executable(wolf_core_data_test tests/core_data_test.c
  WOLFSRC/ID_MM.C WOLFSRC/ID_PM.C WOLFSRC/ID_CA.C WOLFSRC/WL_SCALE.C port/wl_data.c)
target_include_directories(wolf_core_data_test PRIVATE port WOLFSRC)
target_compile_options(wolf_core_data_test PRIVATE ${WOLF3D_TEST_WARNINGS} ${WOLF3D_COMMON_FLAG})
add_test(NAME core_wl1_data COMMAND wolf_core_data_test "${WOLF3D_WEB_TEST_DATA}")
set_tests_properties(core_wl1_data PROPERTIES SKIP_RETURN_CODE 77)

add_executable(wolf_boss_key_test tests/boss_key_test.c WOLFSRC/WL_MENU.C)
target_include_directories(wolf_boss_key_test PRIVATE port WOLFSRC)
target_compile_definitions(wolf_boss_key_test PRIVATE WOLF3D_REGISTERED)
target_compile_options(wolf_boss_key_test PRIVATE ${WOLF3D_TEST_WARNINGS} ${WOLF3D_GAME_FLAGS} ${WOLF3D_SECTION_FLAGS} ${WOLF3D_NODEBUG_OFF})
add_test(NAME registered_boss_key COMMAND wolf_boss_key_test)

add_executable(wolf_sd_test tests/sd_integration_test.c WOLFSRC/ID_SD.C
  WOLFSRC/ID_MM.C WOLFSRC/ID_PM.C WOLFSRC/ID_CA.C port/wl_data.c)
target_include_directories(wolf_sd_test PRIVATE port WOLFSRC)
target_compile_options(wolf_sd_test PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS} ${WOLF3D_NODEBUG_OFF})
target_link_libraries(wolf_sd_test PRIVATE wolf_audio)
add_test(NAME sound_manager COMMAND wolf_sd_test "${WOLF3D_WEB_TEST_DATA}")
set_tests_properties(sound_manager PROPERTIES SKIP_RETURN_CODE 77)

set(WOLF3D_WEB_TESTS_LIST core_codecs core_renderer core_wl1_data registered_boss_key sound_manager)
foreach(t wolf_codec_test wolf_render_test wolf_core_data_test wolf_boss_key_test wolf_sd_test)
  target_link_options(${t} PRIVATE ${WOLF3D_WEB_TEST_LINK})
endforeach()
# node startup plus wasm compile; generous for slow CI runners.
set_tests_properties(${WOLF3D_WEB_TESTS_LIST} PROPERTIES TIMEOUT 120 LABELS web)
