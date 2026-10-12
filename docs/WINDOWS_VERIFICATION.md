# Windows 11 End-to-End Verification Report & Defect Log

## Overview & Test Environment

This document records the end-to-end verification of `raylib_wolf3d` running natively on Windows 11 (x64) as required by WOLF-38 (part of WOLF-29).

### Environment Details
- **Host Machine**: Windows 11 24H2 (`CAL-LAP003447`), interactive session
- **Toolchain**: GCC 15.2.0 (MSYS2 MinGW-w64 x86_64), Ninja 1.12.1, CMake 3.31.5, Python 3.14.4
- **Game Data**: Wolfenstein 3D Shareware v1.4 (`*.WL1`), verified by SHA-256 and kept strictly outside git in `C:\Users\rhernandez\wolf3d-data\shareware`

---

## Subsystem Verification & Evidence

### 1. Launch & Data Path Resolution

#### Tests Executed:
1. **Missing Data Handling**:
   ```powershell
   build-mingw\raylib_wolf3d.exe -data C:\nonexistent
   ```
   **Output & Exit Code**:
   ```text
   raylib_wolf3d: can't use data directory 'C:\nonexistent': No such file or directory
   ExitCode: 2
   ```

2. **Empty Directory Handling**:
   ```powershell
   build-mingw\raylib_wolf3d.exe -data C:\empty_test
   ```
   **Output & Exit Code**:
   ```text
   raylib_wolf3d: AUDIOHED.WL1 not found in 'C:\empty_test'.
   Get the shareware data with scripts/fetch_shareware.sh and pass -data <dir> or set WOLF3D_DATA.
   ExitCode: 2
   ```

3. **Platform Check Test Suite**:
   Executed `tests/platform_check.py` against `tests/platform_test.c`:
   - Argument precedence (`-data`)
   - Relative paths and paths with spaces
   - Environment variables (`WOLF3D_DATA`, `%APPDATA%`, `%LOCALAPPDATA%`)
   - Writable user configuration resolution before changing directory
   **Result**: `PASS: data precedence, spaces, relative paths, fallback, startup errors`

---

### 2. Video & Menu Subsystems

Verified via the built-in video test harness (`-videotest`):
1. **Signon Screen (`-videotest signon`)**:
   - Initializes 320x200 planar mode, sets game palette from linked-in `GAMEPAL.OBJ`, munges `SIGNON.OBJ` and displays signon graphic.
   - **Result**: Window opened, rendered signon screen, waited 4000 ms, closed cleanly (Exit 0).
2. **Title Picture (`-videotest title`)**:
   - Opens `VGAGRAPH.WL1`, decodes Huffman compressed chunk 99 (title picture), and displays it.
   - **Result**: `videotest: using chunk 99 as 320x200 picture #0` (Exit 0).
3. **Palette Fading (`-videotest fade`)**:
   - Tests `VL_FadeOut` (damage flash effect) and `VL_FadeIn`.
   - **Result**: Smooth palette interpolation, clean exit (Exit 0).
4. **Latch Memory (`-videotest latch`)**:
   - Exercises `VL_MemToLatch`, `VL_LatchToScreen`, bars and lines.
   - **Result**: Clean render and exit (Exit 0).
5. **Screen Fizzle (`-videotest fizzle`)**:
   - Tests pseudo-random pixel screen-to-screen fizzle fade used on level start.
   - **Result**: Clean fizzle animation and exit (Exit 0).

---

### 3. Gameplay & Deterministic Demo Synchronization

#### 70 Hz High-Resolution Timer Precision:
```powershell
build-mingw\raylib_wolf3d.exe -data C:\Users\rhernandez\wolf3d-data\shareware -nosound -videotest timing
```
**Output**:
```text
70 x VL_WaitVBL(1): 1000.8 ms, TimeCount advanced 70 (expect ~1000 ms, 70)
busy wait TimeCount 0->140: 1999.2 ms (expect ~2000)
```
*Evaluation*: Monotonic multimedia timer (`QueryPerformanceCounter` + waitable timer) achieved <0.1% jitter on Windows 11.

#### Demo Replay Synchronization (`-demotest all -viewsize 19`):
Plays all 4 shareware demos at full speed, verifying every input tic and matching recorded state hashes:
```powershell
build-mingw\raylib_wolf3d.exe -data C:\Users\rhernandez\wolf3d-data\shareware -nosound -demotest all -viewsize 19
```
**Output**:
```text
demotest: viewsize 19
demotest 0: map=0 frames=1152 left=0 tics=4608 end=died hash=f1e6cd20 x=649081 y=2154816 angle=92 health=0 ammo=45 score=1800 kills=18/37
demotest 1: map=2 frames=1284 left=0 tics=5136 end=died hash=dec12253 x=4075057 y=2938029 angle=135 health=0 ammo=42 score=2000 kills=20/73
demotest 2: map=4 frames=671 left=0 tics=2684 end=died hash=47743191 x=406583 y=2021833 angle=34 health=0 ammo=16 score=2500 kills=13/109
demotest 3: map=6 frames=633 left=0 tics=2532 end=died hash=4b0dd2a9 x=508003 y=3491683 angle=176 health=0 ammo=0 score=1500 kills=7/68
```
*Evaluation*:
- Every demo completed all recorded inputs (`left=0`).
- Every demo ended at the exact player death tick (`end=died`).
- All 4 final state hashes matched the reference recorded values bit-for-bit (`f1e6cd20`, `dec12253`, `47743191`, `4b0dd2a9`).
- Exit code: 0.

#### Direct Level Launch (`-tedlevel 0`):
- Launched directly to Episode 1 Map 1.
- Initialized raycaster, player, actors, walls, doors, sound manager, and ran active `GameLoop()`.

---

### 4. Audio Subsystem

1. **Backend Initialization**:
   - `RL_AudioOpen` initializes raylib miniaudio device at 48 kHz stereo 16-bit.
   - SPSC lock-free audio ring buffer handles audio playback smoothly.
2. **OPL3 Emulation**:
   - Standalone Nuked OPL3 engine generates AdLib music and sound effects correctly.
   - Verified against Wolf4SDL `dc8b250` reference outputs via `audio/tests/compare_reference.py`.
3. **`-nosound` Option**:
   - Cleanly disables audio hardware initialization without errors.

---

### 5. Input Subsystem

1. **Keyboard Translation**:
   - `rl_keys_test` verified 100% translation of raylib keyboard scancodes to DOS XT scancodes.
   - Arrow keys, WASD, modifiers (Ctrl, Alt, Shift), function keys, and keypad mapped correctly.
2. **Event Queue & Tap Detection**:
   - `keyq` circular buffer accurately tracks short taps and key press/release transitions.
3. **Mouse Delta & Buttons**:
   - `RL_MouseDelta` and `RL_MouseButtons` correctly bridge `GetMouseDelta()` and mouse state.

---

### 6. Save / Load & Persistence

#### Save Round-Trip & Validation (`-savetest`):
```powershell
build-mingw\raylib_wolf3d.exe -data C:\Users\rhernandez\wolf3d-data\shareware -nosound -savetest
```
**Output**:
```text
savetest: ok   unchanged file: accepted
savetest: ok   magic + version + checksum only (14 bytes): rejected
savetest: ok   truncated by 100 bytes: rejected
savetest: ok   one trailing byte: rejected
savetest: ok   actor count 0: rejected
savetest: ok   player state index 9999: rejected
savetest: ok   actorat entry 0x7000: rejected
savetest: ok   map number 42: rejected
savetest: ok   health 1000: rejected
savetest: round trip hash 4c492f7f -> 4c492f7f ok
savetest: re-save after load 19079 -> 19079 bytes, identical
```
*Evaluation*:
- Save file serialization round-trip restores game state hash `4c492f7f` exactly.
- Re-saving after load writes identical bytes (19079 -> 19079 bytes).
- 8 corruption test cases rejected by `SaveGameValid` without crashing or corrupting game state.

#### File Replacement Semantics (`WL_ReplaceFile`):
- Uses `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
- Verified that saving over an existing slot replaces the file atomically.

#### User Configuration Persistence (`CONFIG.WL1`):
- Verified in `%APPDATA%\raylib_wolf3d\CONFIG.WL1`.
- Accepts valid config, rejects out-of-range scan codes and button mappings.

---

### 7. Packaging & Distribution

- `scripts/package.ps1` produces `raylib_wolf3d-0.2.0-windows-x64.zip`.
- Automatically bundles required non-system DLLs (`libwinpthread-1.dll`).
- Includes `run.ps1` launcher, `scripts/fetch_shareware.ps1`, licenses, and documentation.
- Generated package verified: unpacked and executed `-videotest timing` and `-savetest` successfully.
- Verified that zero game data (`*.WL1`, `*.SOD`) is contained in the archive.

---

## Defect Log & Evidence

| Defect # | Component | Description & Evidence | Root Cause | Fix / Resolution |
|---|---|---|---|---|
| **DEF-01** | `tests/sd_integration_test.c` | Linker error: `undefined reference to setenv` and `unsetenv` on Windows. | Windows CRT does not provide POSIX `setenv`/`unsetenv`. | Define `_putenv_s(name, value)` and `_putenv_s(name, "")` under `#ifdef _WIN32`. |
| **DEF-02** | `tests/audio_ring_test.c` | Compile error: `redefinition of 'clock_gettime'` on MinGW. | MinGW's `pthread_time.h` defines `clock_gettime` in system headers, colliding with the test's mock function. | Route test clock mock through `WL_TestAudioClock` or `RL_EXTERNAL_CLOCK`. |
| **DEF-03** | `tests/render_test.c` / `wolf_render_test` | Linker error: `undefined reference to s_grdchase1`, `s_ofcchase1`, etc. from `WL_STATE.C.obj`. | On MinGW x86_64 PE/COFF, `-Wl,--gc-sections` does not prune unreferenced `.refptr.*` symbols in `.rdata` generated for unused functions in `WL_STATE.C`. | Provide stubs in `render_test.c` or compile with stubbed actor states on PE targets. |
| **DEF-04** | `tests/persist_test.c` | Compile error: `fatal error: sys/wait.h: No such file or directory`. | `persist_test.c` unconditionally includes `<sys/wait.h>` and calls `fork()`. | Guard `fork()` behind `#ifndef _WIN32` and test `WL_UserFile` in-process or via temporary directory on Windows. |
| **DEF-05** | `tests/persist_check.py` & `tests/demo_sync.cmake` | Test skip: `SKIPPED: xvfb-run not found` when run on Windows. | Test scripts hardcode `xvfb-run` and `XDG_CONFIG_HOME`, assuming a Linux X11 display. | Allow native execution on Windows desktop without `xvfb-run`, using `%APPDATA%` for config isolation. |
| **DEF-06** | `port/wl_persist.c` | Compile error: `conflicting types for 'SetTextColor'` (`wingdi.h` vs `WL_MENU.H`). | Windows `<windows.h>` defines GDI functions unless explicitly suppressed. | `#define WIN32_LEAN_AND_MEAN`, `#define NOGDI`, and `#define NOUSER` before `<windows.h>`. |
| **DEF-07** | `WOLFSRC/WL_MENU.C` / Save games | Overwriting existing save game fails on Windows. | Standard CRT `rename(tmp, path)` fails with `EEXIST` on Windows if destination file exists. | Implement `WL_ReplaceFile()` using `MoveFileExA` with `MOVEFILE_REPLACE_EXISTING`. |

---

## Verdict

- **End-to-End Status**: **PASS**
- The game launches, runs menus, renders gameplay, outputs audio, processes input, and performs save/load operations natively and deterministically on Windows 11.
