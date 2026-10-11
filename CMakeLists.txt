cmake_minimum_required(VERSION 3.16)
project(raylib_wolf3d LANGUAGES C)

# The portable sound manager always links the standalone audio engine.
option(WOLF3D_WITH_AUDIO "Build the audio/ engine (required for the game)" ON)
option(WOLF3D_USE_SYSTEM_RAYLIB "Prefer an installed raylib 5.5 over FetchContent" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# gnu99: the sources define their own boolean {false,true}, which C23 forbids.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ---------------------------------------------------------------------------
# raylib 5.5 (only port/id_raylib.c may include raylib.h)
# ---------------------------------------------------------------------------
set(WOLF3D_RAYLIB_VERSION 5.5)
# The browser build (cmake --preset web, WOLF-41) uses raylib's web platform.
if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "raylib platform" FORCE)
  set(WOLF3D_USE_SYSTEM_RAYLIB OFF)
  # The tests need a display, Xvfb or files outside the page.
  set(BUILD_TESTING OFF CACHE BOOL "Build the tests" FORCE)
endif()
if(WOLF3D_USE_SYSTEM_RAYLIB)
  find_package(raylib ${WOLF3D_RAYLIB_VERSION} EXACT QUIET)
endif()
if(raylib_FOUND)
  message(STATUS "Using system raylib ${raylib_VERSION}")
else()
  message(STATUS "Fetching raylib ${WOLF3D_RAYLIB_VERSION}")
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e)
  FetchContent_MakeAvailable(raylib)
endif()

# ---------------------------------------------------------------------------
# Linked-in data from the original OBJ/ directory (palette, signon screen)
# ---------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WOLF3D_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(WOLF3D_OBJDATA ${WOLF3D_GEN_DIR}/wl_objdata.c)
add_custom_command(
  OUTPUT ${WOLF3D_OBJDATA}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${WOLF3D_GEN_DIR}
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/obj2c.py
          -o ${WOLF3D_OBJDATA}
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/OBJ/GAMEPAL.OBJ
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/OBJ/SIGNON.OBJ
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/obj2c.py
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/OBJ/GAMEPAL.OBJ
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/OBJ/SIGNON.OBJ
  COMMENT "Extracting gamepal/signon from OMF objects"
  VERBATIM)

# ---------------------------------------------------------------------------
# The game: the C files of WOLFSRC/WOLF3D.PRJ (shareware WL1 configuration,
# see WOLFSRC/ID_HEADS.H -> WOLF1VER.H) plus the portable platform layer.
# CONTIGSC.C, OLDSCALE.C, DETECT.C and MUNGE.C are not in the project file.
# ---------------------------------------------------------------------------
set(WOLF3D_GAME_SOURCES
  WOLFSRC/ID_CA.C
  WOLFSRC/ID_IN.C
  WOLFSRC/ID_MM.C
  WOLFSRC/ID_PM.C
  WOLFSRC/ID_SD.C
  WOLFSRC/ID_US_1.C
  WOLFSRC/ID_VH.C
  WOLFSRC/ID_VL.C
  WOLFSRC/WL_ACT1.C
  WOLFSRC/WL_ACT2.C
  WOLFSRC/WL_AGENT.C
  WOLFSRC/WL_DEBUG.C
  WOLFSRC/WL_DRAW.C
  WOLFSRC/WL_GAME.C
  WOLFSRC/WL_INTER.C
  WOLFSRC/WL_MAIN.C
  WOLFSRC/WL_MENU.C
  WOLFSRC/WL_PLAY.C
  WOLFSRC/WL_SCALE.C
  WOLFSRC/WL_STATE.C
  WOLFSRC/WL_TEXT.C
  WOLFSRC/WOLFHACK.C)
set(WOLF3D_PORT_SOURCES
  port/wl_timer.c
  port/wl_videotest.c
  port/wl_compat.c
  port/wl_platform.c
  port/asm_stubs.c
  port/wl_data.c
  port/wl_raycast.c
  port/wl_demotest.c
  port/wl_persist.c)

# Borland C's char was signed, so the build pins -fsigned-char (plain char is
# unsigned on ARM).  WOLF3D_UNSIGNED_CHAR builds with -funsigned-char instead,
# to check that the code no longer depends on it (WOLF-19).
option(WOLF3D_UNSIGNED_CHAR "Build with -funsigned-char (portability check)" OFF)
if(WOLF3D_UNSIGNED_CHAR)
  set(WOLF3D_CHAR_FLAG -funsigned-char)
else()
  set(WOLF3D_CHAR_FLAG -fsigned-char)
endif()
set(WOLF3D_WARNINGS -Wall ${WOLF3D_CHAR_FLAG})
# The id code defines globals in headers (e.g. menuitems in WL_MENU.H) and
# relied on the DOS linker merging them; keep that behaviour.
set(WOLF3D_GAME_FLAGS -fcommon)
# GCC 14+ turned these into errors; the id code (and the DOS-era stubs) still trip them.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
  list(APPEND WOLF3D_GAME_FLAGS -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -Wno-error=implicit-function-declaration)
endif()

# -Wall on the untouched id code is dominated by harmless classes: enum values
# not handled in switch, char signedness, Borland #pragmas, unchecked read()/
# write(), and variables only used by the fenced-off inline asm.  Hide those so
# the classes that find real porting bugs stay visible (implicit declarations,
# incompatible pointers, pointer<->int casts, format, uninitialized).
# -DWOLF3D_ALL_WARNINGS=ON shows the full -Wall output.
option(WOLF3D_ALL_WARNINGS "Do not filter -Wall noise from the original sources" OFF)
if(NOT WOLF3D_ALL_WARNINGS)
  list(APPEND WOLF3D_GAME_FLAGS
    -Wno-switch -Wno-pointer-sign -Wno-unknown-pragmas -Wno-unused-result
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label)
endif()

# Quarantine: id_raylib.c is the only translation unit that sees raylib.h.
# raylib is a PRIVATE dependency of this library, so its include directories
# never reach the game sources.
add_library(wolf3d_raylib STATIC port/id_raylib.c)
target_include_directories(wolf3d_raylib PUBLIC port)
target_link_libraries(wolf3d_raylib PRIVATE raylib)

# The original sources use upper-case .C, which GCC/Clang treat as C++.
set_source_files_properties(${WOLF3D_GAME_SOURCES} PROPERTIES LANGUAGE C)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set_property(SOURCE ${WOLF3D_GAME_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -x c)
endif()

# Table of every actor state, so save games can store states as indices.
set(WOLF3D_STATES ${WOLF3D_GEN_DIR}/wl_states.c)
add_custom_command(
  OUTPUT ${WOLF3D_STATES}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${WOLF3D_GEN_DIR}
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_states.py > ${WOLF3D_STATES}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_states.py
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/WL_AGENT.C
          ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC/WL_ACT2.C
  COMMENT "Generating the actor state table"
  VERBATIM)

# Separate executables preserve the original UPLOAD/GOODTIMES code paths.
# Keep the established WL1 target/name for scripts and demo_sync.
add_executable(raylib_wolf3d ${WOLF3D_GAME_SOURCES} ${WOLF3D_PORT_SOURCES} ${WOLF3D_OBJDATA} ${WOLF3D_STATES})
add_executable(wolf3d-wl6 ${WOLF3D_GAME_SOURCES} ${WOLF3D_PORT_SOURCES} ${WOLF3D_OBJDATA} ${WOLF3D_STATES})
target_compile_definitions(wolf3d-wl6 PRIVATE WOLF3D_REGISTERED)
if(NOT WOLF3D_WITH_AUDIO)
  message(FATAL_ERROR "The game requires the audio engine; use -nosound at runtime")
endif()
add_subdirectory(audio)
target_compile_definitions(wolf3d_raylib PRIVATE WOLF3D_AUDIO_BACKEND)
option(WOLF3D_SANITIZERS "Build the games with ASan/UBSan" OFF)
if(EMSCRIPTEN)
  include(web/web.cmake)
else()
  find_package(Threads REQUIRED)
endif()
foreach(game raylib_wolf3d wolf3d-wl6)
  target_include_directories(${game} PRIVATE port WOLFSRC)
  target_link_libraries(${game} PRIVATE wolf3d_raylib wolf_audio m)
  if(EMSCRIPTEN)
    wolf3d_configure_web(${game})
  else()
    target_link_libraries(${game} PRIVATE Threads::Threads)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${game} PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS})
    if(WOLF3D_SANITIZERS)
      target_compile_options(${game} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${game} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endforeach()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(wolf3d_raylib PRIVATE ${WOLF3D_WARNINGS})
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME registered_stub_guard COMMAND Python3::Interpreter -I
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/registered_stub_guard.py
    ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(registered_stub_guard PROPERTIES TIMEOUT 30)
  add_executable(wolf_boss_key_test tests/boss_key_test.c WOLFSRC/WL_MENU.C)
  target_include_directories(wolf_boss_key_test PRIVATE port WOLFSRC)
  target_compile_definitions(wolf_boss_key_test PRIVATE WOLF3D_REGISTERED)
  target_compile_options(wolf_boss_key_test PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS}
    -UNDEBUG -ffunction-sections -fdata-sections)
  target_link_options(wolf_boss_key_test PRIVATE -Wl,--gc-sections)
  add_test(NAME registered_boss_key COMMAND wolf_boss_key_test)
  set_tests_properties(registered_boss_key PROPERTIES TIMEOUT 5)
  add_executable(wolf_wl6_layout tests/wl6_layout.c)
  target_include_directories(wolf_wl6_layout PRIVATE port WOLFSRC)
  target_compile_definitions(wolf_wl6_layout PRIVATE WOLF3D_REGISTERED)
  target_compile_options(wolf_wl6_layout PRIVATE ${WOLF3D_WARNINGS} -fcommon)
  add_test(NAME synthetic_wl6 COMMAND Python3::Interpreter -I
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/synthetic_wl6.py
    $<TARGET_FILE:wolf3d-wl6> $<TARGET_FILE:wolf_wl6_layout>
    $<TARGET_FILE:raylib_wolf3d>)
  set_tests_properties(synthetic_wl6 PROPERTIES TIMEOUT 60)
  add_executable(wolf_core_data_test tests/core_data_test.c
    WOLFSRC/ID_MM.C WOLFSRC/ID_PM.C WOLFSRC/ID_CA.C WOLFSRC/WL_SCALE.C port/wl_data.c)
  target_include_directories(wolf_core_data_test PRIVATE port WOLFSRC)
  target_compile_options(wolf_core_data_test PRIVATE -Wall -Wextra ${WOLF3D_CHAR_FLAG} -fcommon)
  add_executable(wolf_codec_test tests/codec_test.c port/wl_data.c)
  target_include_directories(wolf_codec_test PRIVATE port)
  target_compile_options(wolf_codec_test PRIVATE -Wall -Wextra ${WOLF3D_CHAR_FLAG})
  add_executable(wolf_render_test tests/render_test.c WOLFSRC/WL_DRAW.C WOLFSRC/WL_STATE.C
    WOLFSRC/WL_SCALE.C WOLFSRC/WOLFHACK.C WOLFSRC/ID_MM.C port/wl_raycast.c port/wl_data.c port/asm_stubs.c)
  target_include_directories(wolf_render_test PRIVATE port WOLFSRC)
  target_compile_options(wolf_render_test PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS} -ffunction-sections -fdata-sections)
  target_link_options(wolf_render_test PRIVATE -Wl,--gc-sections)
  target_link_libraries(wolf_render_test PRIVATE m)
  add_test(NAME core_renderer COMMAND wolf_render_test)
  set(WOLF3D_TEST_DATA "$ENV{HOME}/wolf3d-data/shareware" CACHE PATH "External WL1 data for core tests")
  add_test(NAME core_wl1_data COMMAND wolf_core_data_test "${WOLF3D_TEST_DATA}")
  set_tests_properties(core_wl1_data PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
  add_test(NAME core_codecs COMMAND wolf_codec_test)

  # The four shareware demos, at full speed under Xvfb, against recorded
  # final-state hashes (WOLF-7).  Skips without WL1 data or xvfb-run.
  add_test(NAME demo_sync COMMAND ${CMAKE_COMMAND}
    -DGAME=$<TARGET_FILE:raylib_wolf3d> -DDATA=${WOLF3D_TEST_DATA}
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/demo_sync
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_sync.cmake)
  set_tests_properties(demo_sync PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED" TIMEOUT 300)

  # Config and save game formats: -savetest plus malformed CONFIG.WL1 files.
  add_test(NAME persist_check COMMAND Python3::Interpreter -I
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/persist_check.py
    $<TARGET_FILE:raylib_wolf3d> ${WOLF3D_TEST_DATA} ${CMAKE_CURRENT_BINARY_DIR}/persist_check)
  set_tests_properties(persist_check PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED" TIMEOUT 600)

  # The persistence streams and paths on their own: no data, no display (WOLF-18).
  add_executable(wolf_persist_test tests/persist_test.c port/wl_persist.c)
  target_include_directories(wolf_persist_test PRIVATE port WOLFSRC)
  target_compile_options(wolf_persist_test PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS})
  add_test(NAME core_persist COMMAND wolf_persist_test)

  # ---------------------------------------------------------------------------
  # Tests (ctest): the raylib keyboard translation against a fake raylib, no display needed.
  # ---------------------------------------------------------------------------
  add_executable(rl_keys_test tests/rl_keys_test.c port/id_raylib.c)
  target_include_directories(rl_keys_test PRIVATE port
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(rl_keys_test PRIVATE m)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rl_keys_test PRIVATE -Wall)
  endif()
  add_test(NAME rl_keys_test COMMAND rl_keys_test)

  option(WOLF3D_CORE_SANITIZERS "Use ASan/UBSan on headless core tests" OFF)
  if(WOLF3D_CORE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(t wolf_core_data_test wolf_codec_test wolf_render_test wolf_persist_test rl_keys_test)
      target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endforeach()
  endif()
endif()

if(BUILD_TESTING)
  foreach(t wolf_sd_test wolf_sd_device)
    add_executable(${t} tests/sd_integration_test.c WOLFSRC/ID_SD.C
      WOLFSRC/ID_MM.C WOLFSRC/ID_PM.C WOLFSRC/ID_CA.C port/wl_data.c)
    target_include_directories(${t} PRIVATE port WOLFSRC)
    target_compile_options(${t} PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS} -UNDEBUG)
    target_link_libraries(${t} PRIVATE wolf_audio)
  endforeach()
  target_compile_definitions(wolf_sd_device PRIVATE SD_DEVICE)
  target_link_libraries(wolf_sd_device PRIVATE wolf3d_raylib)
  add_test(NAME sound_manager COMMAND wolf_sd_test "${WOLF3D_TEST_DATA}")
  set_tests_properties(sound_manager PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()
if(BUILD_TESTING)
  add_executable(wolf_audio_ring_test tests/audio_ring_test.c port/id_raylib.c)
  target_include_directories(wolf_audio_ring_test PRIVATE port
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(wolf_audio_ring_test PRIVATE WOLF3D_AUDIO_BACKEND)
  target_compile_options(wolf_audio_ring_test PRIVATE -Wall -UNDEBUG)
  target_link_libraries(wolf_audio_ring_test PRIVATE m)
  add_test(NAME audio_ring COMMAND wolf_audio_ring_test)
endif()
