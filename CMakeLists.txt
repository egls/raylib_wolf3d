cmake_minimum_required(VERSION 3.16)
project(raylib_wolf3d LANGUAGES C)

# Stage 3 (WOLF-3, PR #1) adds a standalone audio/ project. Once it is on
# master, enable it here and link wolf_audio into raylib_wolf3d.
option(WOLF3D_WITH_AUDIO "Build the audio/ engine (WOLF-3)" OFF)
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
  port/wl_compat.c
  port/wl_platform.c
  port/asm_stubs.c
  port/wl_data.c
  port/wl_raycast.c)

set(WOLF3D_WARNINGS -Wall -fsigned-char)
# The id code defines globals in headers (e.g. menuitems in WL_MENU.H) and
# relied on the DOS linker merging them; keep that behaviour.
set(WOLF3D_GAME_FLAGS -fcommon)

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

add_executable(raylib_wolf3d ${WOLF3D_GAME_SOURCES} ${WOLF3D_PORT_SOURCES} ${WOLF3D_OBJDATA})
target_include_directories(raylib_wolf3d PRIVATE port WOLFSRC)
target_link_libraries(raylib_wolf3d PRIVATE wolf3d_raylib m)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(raylib_wolf3d PRIVATE ${WOLF3D_WARNINGS} ${WOLF3D_GAME_FLAGS})
  target_compile_options(wolf3d_raylib PRIVATE ${WOLF3D_WARNINGS})
endif()

if(WOLF3D_WITH_AUDIO)
  add_subdirectory(audio)
  target_link_libraries(raylib_wolf3d PRIVATE wolf_audio)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(wolf_core_data_test tests/core_data_test.c
    WOLFSRC/ID_MM.C WOLFSRC/ID_PM.C WOLFSRC/ID_CA.C WOLFSRC/WL_SCALE.C port/wl_data.c)
  target_include_directories(wolf_core_data_test PRIVATE port WOLFSRC)
  target_compile_options(wolf_core_data_test PRIVATE -Wall -Wextra -fsigned-char -fcommon)
  add_executable(wolf_codec_test tests/codec_test.c port/wl_data.c)
  target_include_directories(wolf_codec_test PRIVATE port)
  target_compile_options(wolf_codec_test PRIVATE -Wall -Wextra -fsigned-char)
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
  option(WOLF3D_CORE_SANITIZERS "Use ASan/UBSan on headless core tests" OFF)
  if(WOLF3D_CORE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(t wolf_core_data_test wolf_codec_test wolf_render_test)
      target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endforeach()
  endif()
endif()
