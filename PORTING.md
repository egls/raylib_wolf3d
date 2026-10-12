# Porting inventory

The WOLF-7 section (first playable) is at the end of this file.

The inventory below records the WOLF-2 foundation. The WOLF-4 update immediately below supersedes its S2-core stub entries.

This file lists everything in `WOLFSRC/` that does not run on a 64-bit OS as written, what WOLF-2 did about it, and which stage replaces it.

Owners:

- **S2-core**: Stage 2 engine core (Wolf Engine Porter). Asm translations, ID_MM/ID_PM/ID_CA, data loading, 16-bit type decisions.
- **S2-backend**: Stage 2 backend (Wolf Builder). ID_VL/ID_VH video, ID_IN input, timer, through `port/id_raylib.c`.
- **S3-audio**: Stage 3 ID_SD (Wolf Audio Porter), on top of the WOLF-3 `audio/` engine (PR #1).

WOLF-2 only makes the program build and link. Anything DOS-only aborts at runtime with `raylib_wolf3d: not ported yet: <what> (<file>:<line>)` and exit code 3. It never silently returns.

## WOLF-4 core implementation

- `ID_MM.C` uses malloc with stable owner slots, purge invalidation and locks.
  Sort purges unlocked purgeable allocations, calls the original hooks, and
  never moves live blocks. Memory totals are advisory OS availability.
- `ID_PM.C` reads the VSWAP header/offsets/lengths explicitly and retains all
  non-sparse pages; EMS/XMS are absent. Lock/purge APIs remain callable because
  resident pages cannot be evicted. Shutdown releases them separately from MM.
- `ID_CA.C` decodes 38-byte map headers, 24-bit graphics offsets, 16-bit
  Huffman nodes, and 32-bit audio offsets field by field. Carmack/RLEW/Huffman
  production decoders in `port/wl_data.c` reject input/output overruns. Map
  planes and their game callers use `word *`. Pictures and sound headers are
  constructed in native memory; fonts/music have fixed-size native word fields.
- `GFXV_WL14.H` matches the SHA-pinned Apogee v1.4 WL1 set: 156 chunks,
  144 pictures, **35** eight-pixel tiles, eight externals. Title is chunk 99,
  tiles are chunk 147, externals start at 148. The Apogee names/ordering were
  cross-checked with [Wolf4SDL's Apogee header](https://github.com/fabiangreffrath/wolf4sdl/blob/master/gfxv_apo.h);
  the actual tile count and decoded lengths were checked against our WL1 files.
- `port/wl_raycast.c` translates `WL_DR_A.ASM`, including original quadrant
  lookup/equality ordering, midpoint door tests, pushwall multiplication with
  32-bit wrap, and partial products truncating toward zero. `FixedByFrac`
  preserves the trig table's signed-magnitude convention (not two's complement).
- `WL_SCALE.C` uses loops with original integer pixel edges and large-height
  aliases instead of generated code. Sprite commands are checked byte streams;
  virtual pixel bases wrap as 16-bit offsets before indexing the page.
- `WL_DRAW.C` uses flat wall-page pointers and `VL_Plot`/`VL_Hlin`/`VL_SetScreen`.
  These existing backend functions keep their signatures and relative
  `bufferofs` coordinates; no `raylib.h` enters the core. The first wall/door/
  pushwall post cannot reuse stale state from a previous frame.
- `WOLFHACK.C`/`MapRow` use the same backend pixel API. RNG table/order are
  retained. CPU detection selects native division; there is no code patching.
- Actor occupancy retains full `uintptr_t` values in `TryWalk`. Menu callbacks
  have correctly typed adapters. Signon static storage is not reclaimed by
  paragraph arithmetic; errors print before teardown rather than recursively
  requesting a cache chunk during a loader failure.

Validation: CTest `core_codecs`, `core_renderer`, `core_wl1_data` (the last
skips without external WL1 data). Sanitizers are optional via
`WOLF3D_CORE_SANITIZERS`. No commercial data is used or included.
WOLF-5 video/input is integrated, including its keyboard test under sanitizers.
Xvfb WL1 startup passes video setup and stops at the WOLF-6 `SDL_SetDS` audio
adapter (exit 3); the title video harness renders chunk 99 and exits 0.
Gameplay and demo sync still require the audio/timer integration.
`CONFIG`/save-game native dumps remain a separate persistence migration; the
core's game-data readers no longer use native disk struct dumps.

## Build configuration

| Item | Choice | Where |
|---|---|---|
| Version header | `WOLF1VER.H` (`UPLOAD`, `ARTSEXTERN`, `DEMOSEXTERN`, `CARMACIZED`; no `SPEAR`, `JAPAN` or `GOODTIMES`) | `ID_HEADS.H`, `ID_HEAD.H` (both used to include `VERSION.H`, the registered config) |
| Audio header | `AUDIOWL6.H` | `ID_HEADS.H` |
| Map header | `MAPSWL6.H` | `ID_HEADS.H` |
| Graphics header | `GFXV_WL6.H` (**placeholder**) | `ID_HEADS.H` |
| Source list | the C files of `WOLF3D.PRJ` | `CMakeLists.txt` |

The `*WL1.H` headers in this release are older than shareware v1.4 and don't match its data:

- `AUDIOHED.WL1` (v1.4) has 288 chunks. `AUDIOWL6.H` also has 288 (87 sounds); `AUDIOWL1.H` has 234.
- `VGAHEAD.WL1` (v1.4) has 156 chunks. `GFXV_WL1.H` says 556 and `GFXV_WL6.H` says 149.
- **S2-core must add a graphics header that matches the v1.4 shareware `VGAGRAPH` before any picture is drawn.** Until then chunk numbers are wrong.

Not built (not in `WOLF3D.PRJ`): `CONTIGSC.C`, `OLDSCALE.C` (older scalers), `DETECT.C`, `MUNGE.C`, `JABHACK.ASM`.

## Assembly modules (`.ASM`)

Each routine is replaced by a stub in `port/asm_stubs.c` with the same C signature. Data tables are carried over as data.

| File | Provides | WOLF-2 | Owner |
|---|---|---|---|
| `C0.ASM` | Borland startup, `_argc/_argv`, exit handling, null check, divide-by-zero handler | dropped. C runtime + `main()`; `_argc/_argv` set in `port/wl_platform.c` | done |
| `H_LDIV.ASM` | `LDIV@`, `LUDIV@`, `LMOD@`, `LUMOD@` (32-bit divide helpers) | dropped (native) | done |
| `ID_VL_A.ASM` | `VL_WaitVBL`, `VL_SetCRTC`, `VL_SetScreen`, `VL_ScreenToScreen`, `VL_VideoID` | **done (WOLF-5)**, C in `ID_VL.C` | done |
| `ID_VH_A.ASM` | `VH_UpdateScreen` (dirty-tile blit to VGA) | **done (WOLF-5)**, C in `ID_VH.C` | done |
| `ID_US_A.ASM` | `US_InitRndT`, `US_RndT`, `rndtable[256]` | stubs; `rndtable` copied verbatim | S2-core |
| `ID_SD_A.ASM` | `SDL_SetDS`, `SDL_IndicatePC`, `SDL_t0ExtremeAsmService`, `SDL_t0FastAsmService`, `SDL_t0SlowAsmService` (timer ISRs, PC speaker/AdLib/digitized) | stubs | S3-audio |
| `WL_DR_A.ASM` | `AsmRefresh` (raycaster inner loop; near helpers `xpartialbyystep`, `ypartialbyxstep`) | stub | S2-core |
| `WHACK_A.ASM` | `MapRow`, `planepics[8192]` (floor/ceiling) | stub; `planepics` defined | S2-core |
| `WL_ASM.ASM` | `CheckIs386`, `jabhack2` (called from `Patch386`) | stubs. **First abort a run hits today.** | S2-core |
| `OBJ/GAMEPAL.OBJ`, `OBJ/SIGNON.OBJ` | `gamepal` (768 B), `signon` (64000 B), linked in | turned into C at build time by `scripts/obj2c.py` | done |

## Inline asm in C

Every run of Borland `asm` statements in the built C files is kept in place under `#if 0`, preceded by `PORT_ASM();` (aborts with the enclosing function name). Header macros that expand to asm now expand to `PORT_ASM()`.

| File | Fenced runs | What the asm does | Owner |
|---|---:|---|---|
| `ID_SD.C` | 60 | PIT/timer programming, PC speaker, AdLib/SB port I/O, ISR glue | S3-audio |
| `ID_PM.C` | 12 | EMS (`int 67h`) / XMS driver calls | S2-core |
| `ID_VL.C` | 10 | VGA mode set, palette I/O, planar memory copies | done (WOLF-5: rewritten, see below) |
| `ID_CA.C` | 9 | Huffman expand, RLEW/Carmack helpers, far reads | S2-core |
| `WL_DRAW.C` | 9 | `ScalePost`, `FixedByFrac`, `TransformTile`/wall helpers, VGA latches | S2-core |
| `ID_VH.C` | 5 | latch/plane copies, `FizzleFade` | done (WOLF-5) |
| `ID_MM.C` | 3 | XMS UMB allocation, segment math | S2-core |
| `ID_US_1.C` | 3 | BIOS text output, `int 21h` | S2-backend |
| `WL_SCALE.C` | 2 | compiled-scaler call glue | S2-core |
| `ID_IN.C` | 1 | mouse/joystick BIOS calls | done (WOLF-5) |
| `WL_DEBUG.C` | 1 | debug border/port I/O | S2-backend |
| `ID_VL.H` macros | `VGAWRITEMODE`, `VGAMAPMASK`, `VGAREADMAP` | VGA register writes | done (WOLF-5: no-ops, memory is linear) |
| `WL_DEF.H` macro | `COLORBORDER` | overscan colour | done (WOLF-5: `VL_ColorBorder`, remembered only) |
| `ID_MM.H` macro | `XMS_CALL` | far call into XMS driver | S2-core |

`WL_SCALE.C` `BuildCompScale` writes x86 machine code at runtime (compiled scalers). The emitting line now calls `NOTPORTED("BuildCompScale: x86 code generation")`. S2-core must replace it with a C scaler.

## DOS / BIOS / Borland runtime APIs

All are declared in `port/wl_compat.h` and defined in `port/wl_compat.c`.

| API | Used in | WOLF-2 | Owner |
|---|---|---|---|
| `MK_FP`, `FP_SEG`, `FP_OFF` (45 uses) | ID_CA, ID_VL, WL_DRAW, WL_SCALE, ID_MM, ID_PM, ID_VH, WL_DEBUG, WL_MAIN, ID_IN | abort | S2-core / S2-backend |
| `inportb`, `outportb`, `outport` (74 uses) | mostly ID_VL and ID_SD; also ID_IN, WL_DRAW, WOLFHACK | abort | S2-backend / S3-audio |
| `geninterrupt`, `int86`, `union REGS`, pseudo-registers `_AX`... | ID_IN, ID_PM, ID_VL, ID_US_1, WL_MAIN, WL_MENU | abort; registers are plain globals so loads compile | S2-backend / S2-core |
| `getvect`, `setvect`, `interrupt` functions, `disable`, `enable` | ID_SD (timer ISR), ID_IN (keyboard ISR) | abort; `interrupt` is an empty macro | S2-backend (keyboard, timer), S3-audio |
| `peek`, `peekb`, `poke` | ID_IN, ID_US_1, WL_MAIN (BIOS data area, VGA memory) | abort | S2-backend |
| `farcoreleft`, `coreleft` | ID_MM | abort | S2-core |
| `farmalloc`, `farfree` | ID_MM | `malloc`/`free` | S2-core (allocator redesign) |
| `movedata` | ID_MM (EMS/XMS copies), WL_MAIN (text screen) | abort | S2-core / S2-backend |
| `harderr` | ID_US_1 | abort | S2-core |
| `bioskey` | WL_DEBUG | abort | S2-backend |
| `_dos_getdate`, `_dos_gettime`, `_dos_getdiskfree`, `_dos_write` | WL_MAIN (BETA only), WL_MENU (save games) | abort | S2-core |
| `gotoxy`, `clrscr` (conio) | WL_MAIN (quit/error text screen) | abort | S2-backend |
| `_fmemcpy`, `_fmemset`, `_fmemcmp`, `_fstrcpy`, `_fstrlen`, `_fstricmp`, `stricmp` | many | mapped to the flat versions | done |
| `itoa`, `ltoa`, `ultoa`, `filelength`, `O_BINARY`, `S_IREAD/S_IWRITE`, `EINVFMT`, `MAXINT/MAXLONG` | many | implemented | done |
| `findfirst`/`findnext` (`struct ffblk`) | WL_MENU (`CheckForEpisodes`), save-game list | implemented: case-insensitive match in the current directory | done |
| `open/read/write/lseek/close` | ID_CA, ID_PM, WL_MAIN, WL_MENU | POSIX; 3 `open(...O_CREAT...)` calls got the missing mode argument | done |

## `far` / `huge` / `near` / `_seg` / `interrupt`

These are empty macros in `wl_compat.h`. Uses in the built C files and all headers: `far` about 430, `_seg` 42 (ID_CA 14, ID_VH 8, WL_SCALE 7, WL_TEXT 4, ...), `huge` 17, `interrupt` 19 (ID_SD 15, ID_IN 4).

`_seg` pointers are now ordinary pointers. Code that builds them from `FP_SEG` or assumes 16-byte paragraphs (ID_MM, ID_PM, ID_CA, sprite/`t_compshape` access) must be rewritten by S2-core.

Borland cast-as-lvalue idioms were rewritten so they compile:

- `(unsigned)actorat[x][y] = v` became `actorat[x][y] = (objtype *)(uintptr_t)(v)` (14 sites, faithful). Reads use `(unsigned)(uintptr_t)actorat[...]`.
- `&(memptr)p` and `&((memptr)p)` became `(memptr *)&p` (faithful).
- `(long)ptr = 0` became `ptr = 0` (faithful).
- `*((unsigned char far *)inptr)++` in Carmack expansion is now an explicit cast and step (byte-exact). `*((unsigned far *)demoptr)++` in demo playback is now a little-endian 16-bit read (byte-exact).
- `(unsigned)postsource = ofs` / `(unsigned)linecmds = ofs` and the matching segment stores are now `SET_FP_OFF` / `SET_FP_SEG`, which **abort**. They only make sense with segmented pointers. Owner: S2-core (WL_DRAW, WL_SCALE).

## 64-bit and type-size issues

| Issue | WOLF-2 | Owner |
|---|---|---|
| `word` was `unsigned int` (16-bit) | now `uint16_t` (`ID_HEADS.H`, `ID_HEAD.H`) | done |
| `longword` was `unsigned long` (32-bit) | now `uint32_t` | done |
| `fixed` was `long` (16.16) | now `int32_t` (`WL_DEF.H`) | done |
| bare `int`/`unsigned` assumed 16-bit (wraparound in timers and offsets, `unsigned far *` word streams in ID_CA Carmack/RLEW, `MAXINT` use) | **unchanged**; `MAXINT` keeps the 16-bit value | S2-core |
| bare `long` assumed 32-bit (`heightnumerator`, `postsource`, file offsets, `maptype.planestart`) | **unchanged** (64-bit on LP64) | S2-core |
| pointer/int casts (`-Wpointer-to-int-cast` 14, `-Wint-to-pointer-cast` 22): ID_PM, ID_CA, ID_MM, WL_MAIN, WL_DEBUG, WL_SCALE, WOLFHACK, WL_STATE | warnings left visible | S2-core |
| `word *` vs `unsigned *` mismatches now that the two differ: `IN_GetJoyAbs` (WL_MENU), `VW_MeasurePropString` (WL_TEXT), `PM_Preload` callback (WL_INTER) | warnings left visible | S2-core / S2-backend |
| menu callbacks `void (*)(int)` given `void (*)(void)` / `int (*)(int)` (WL_MENU, WL_GAME) | warnings left visible (works on the target ABIs, still UB) | S2-core |
| `gamepal`/`signon` declared as scalars (`extern byte far gamepal;`) but defined as arrays by the generated file | links fine; tidy up with the palette work | S2-backend |
| `VL_TestPaletteSet` writes `palette1[0][0..767]` (deliberate overflow of the first row) | `-Waggressive-loop-optimizations` warning | S2-backend |
| globals defined in headers (`menuitems` in WL_MENU.H, `update`) | built with `-fcommon`, as the DOS linker did | done |
| `char` signedness | `-fsigned-char`, matching Borland | done |
| missing prototypes (implicit `int` returns) | added for `PG13`, `SpawnBJVictory`, `A_DeathScream`, `FreeMusic`, `VL_LatchToScreen`, `VH_UpdateScreen`, `US_CheckParm` (ID_VL.C), `MapRow` | done |

## On-disk data (packing / endianness)

None of these are read portably yet. Every one is read with `read(fd, &struct, sizeof struct)`, which depends on 16-bit `int` and no padding. S2-core must read them field by field as little-endian.

| Data | Structure | File |
|---|---|---|
| `MAPHEAD.WL1` / `GAMEMAPS.WL1` | `mapfiletype` (`unsigned RLEWtag; long headeroffsets[100]`), `maptype` (`long planestart[3]; unsigned planelength[3]; unsigned width,height; char name[16]`) | ID_CA.C, ID_CA.H |
| `VGAHEAD/VGADICT/VGAGRAPH.WL1` | 3-byte offsets, `huffnode` (`unsigned bit0,bit1`), `pictabletype`, `spritetabletype` | ID_CA.C, ID_VH.H |
| `VSWAP.WL1` | `word ChunksInFile, PMSpriteStart, PMSoundStart`, `longword` offsets, `word` lengths; `t_compshape` sprites | ID_PM.C, WL_DEF.H |
| `AUDIOHED/AUDIOT.WL1` | `long` offsets; `SoundCommon`, `PCSound`, `AdLibSound`, `Instrument`, `MusicGroup` | ID_CA.C, ID_SD.H |
| `CONFIG.WL1` | high scores, enum-sized `SDMode`/`SMMode`/`SDSMode`, `boolean` flags, scan codes | WL_MAIN.C `ReadConfig`/`WriteConfig` |
| `SAVEGAM?.WL1` | raw dumps of `gametype`, `objtype` (contains pointers), level state | WL_MENU.C, WL_GAME.C |

The data is little-endian. Every current target (x86_64, aarch64) is little-endian, but the readers should not depend on that.

## Platform layer added by WOLF-2

| File | Role |
|---|---|
| `port/wl_compat.h` / `.c` | Borland/DOS replacements and the not-ported aborts |
| `port/wl_platform.c` | `WL_PlatformStartup`: `_argc/_argv`, data directory (`-data <dir>`, `WOLF3D_DATA`, or `.`), WL1 file check, window; `WL_NotPorted` |
| `port/asm_stubs.c` | `.ASM` routine stubs and data tables |
| `port/id_raylib.h` / `.c` | the only raylib user, behind a plain-C API (window open/hold/close for now) |
| `scripts/obj2c.py` | `OBJ/*.OBJ` (OMF) to C arrays, run by CMake |

Startup order today: `main` → `WL_PlatformStartup` (window opens) → `CheckForEpisodes` (finds `*.WL1`) → `Patch386` → `CheckIs386` → abort.

## Video, input and timer backend (WOLF-5)

Replaces the DOS video, keyboard/mouse/joystick and timer layers.  `raylib.h` is still only included by `port/id_raylib.c`.

### Video memory model: read this before touching the renderer

Wolf3D drew into VGA Mode X: 4 planes of 64K, pages and latch pictures addressed in plane bytes, writes steered by the map mask.  The port keeps those **addresses** (`bufferofs`, `displayofs`, `linewidth`, `ylookup[]`, `latchpics[]`, `freelatch`, `PAGE1START` ... are unchanged, in plane bytes) but stores the memory linearly: plane byte `o` of plane `p` is `vl_vram[4*o + p]`. So pixel `(x,y)` of the page at plane offset `ofs` is

```
vl_vram[4*(ofs + ylookup[y]) + x]
```

and a latch (the 4 plane bytes at one address) is 4 consecutive bytes. Consequences:

- `VGAMAPMASK`, `VGAWRITEMODE`, `VGAREADMAP` are no-ops. Code that selects planes with them (the wall/sprite scalers in `WL_DRAW.C`, `WL_SCALE.C`, `WOLFHACK.C`, `WL_DR_A.ASM`) must write chunky pixels with the formula above instead; `VL_Span(ofs, y, x, count)` gives a checked pointer.
- Everything in `ID_VL.C` / `ID_VH.C` that touched `SCREENSEG` is translated: `VL_Plot/Hlin/Vlin/Bar`, `VL_MemToScreen` (de-planes a `VL_MungePic`-ordered picture), `VL_MemToLatch`, `VL_LatchToScreen`, `VL_ScreenToScreen`, `VL_DrawTile8String`, `VL_DrawLatch8String`, `VW_DrawPropString`, `VW_DrawColorPropString`, `VH_UpdateScreen`, `FizzleFade`.
- `MK_FP(SCREENSEG, ...)` still aborts: S2-core code that builds screen pointers must use `vl_vram`.
- The page that is shown is the CRTC start address set by `VL_SetScreen` / `VL_SetCRTC` (not `displayofs`, which the game keeps for itself, as before).

Palette: `VL_SetPalette/GetPalette/SetColor/FillPalette/FadeIn/FadeOut` keep the 6-bit VGA DAC values; `VL_Present` expands the shown page through a 256-entry RGBA table. Damage/bonus flashes (`VL_SetPalette(redshifts[..])`) work unchanged. `VL_ColorBorder` only remembers the colour.

### Presenting, timing, input

| Piece | Where | Notes |
|---|---|---|
| Present | `ID_VL.C` `VL_Present`/`VL_Refresh`, `port/id_raylib.c` `RL_Present` | `VL_WaitVBL(n)` sleeps to the n-th 70 Hz boundary and presents. `VL_SetScreen`, `VH_UpdateScreen` and `IN_PumpEvents` also present (rate limited to 70 Hz), so busy loops keep the window alive. Scaling: whole-number nearest enlargement into a render texture, then smooth scale to the window, letterboxed 4:3. Alt+Enter toggles borderless fullscreen (both Enter events are swallowed). |
| `TimeCount` | `port/wl_timer.c` | A thread adds ticks from `CLOCK_MONOTONIC` at 70 Hz; `TimeCount` is now `_Atomic` (`ID_SD.H/.C`), so the tick thread and the game never race. Started by `WL_PlatformStartup`. Wolf's `while (TimeCount < n);` loops and `TimeCount = 0` resets work unchanged. The 700 Hz music/effects timer is **not** here: S3-audio drives `ID_SD` from the audio callback. |
| Keyboard | `ID_IN.C` `IN_PumpEvents`, `id_raylib.c` | raylib is polled; key changes become PC set-1 scan codes (0xE0 prefix for the extended keys) fed to the old `INL_KeyService`, which still maintains `Keyboard[]`, `LastScan`, `LastASCII` and the key hook. raylib's pressed-key queue is drained before and after every poll (EndDrawing resets it) so taps shorter than a poll are kept. |
| Waiting on input | `IN_SpinWait()` | Every `while (Keyboard[..]);`, `while (!LastScan);` and `while (TimeCount < n);` loop in `ID_IN`, `WL_MENU`, `WL_TEXT`, `WL_INTER`, `WL_DEBUG` now calls it. New code that spins on those variables must too. |
| Mouse | `IN_MouseCall` (the `Mouse(x)` macro) | Emulates int 33h functions 0, 3, 4, 11 on `RL_MouseDelta/Buttons`; the cursor is captured (hidden, locked) only inside `PlayLoop`. |
| Joystick | `IN_GetJoyAbs`, `IN_JoyButtons` | Gamepad 0: left stick/d-pad as the analog stick (1000 +- 1000 counts), A/right trigger = button 0, B/X = button 1. |
| Closing the window | `IN_PumpEvents` | Calls `Quit(NULL)` (saves the config, like the menu's Quit) since WOLF-7. |

### Not done here

- `SignonScreen` / `VL_MungePic` still go through `MM_*` and `FP_SEG` (S2-core).
- No graphics header matches the v1.4 shareware `VGAGRAPH` yet (S2-core), so the title picture is only shown by the harness below.

### Developer harness and tests

- `raylib_wolf3d -data DIR -videotest signon|title|fade|latch|fizzle|keys|timing` runs `port/wl_videotest.c`: it shows the linked-in signon screen, the title picture decoded straight from `VGAGRAPH.WL1`, a palette fade, latch/bar/line drawing, `FizzleFade`, prints key events, or measures `VL_WaitVBL` and `TimeCount` against the clock.
- `ctest` runs `tests/rl_keys_test.c`: `port/id_raylib.c` against a scripted fake raylib (held keys, sub-poll taps, extended keys, Alt+Enter), no display needed.
- GCC 14+ makes incompatible-pointer-type, int-conversion and implicit-declaration errors; the game sources are built with `-Wno-error=` for those three until S2-core clears the warnings.

## First playable (WOLF-7)

### Sound

`port/sd_silent.c`, the temporary no-sound Sound Manager used while WOLF-6 was
open, is gone: `WOLFSRC/ID_SD.C` (WOLF-6) is built again.  Known coupling to
keep in mind: `UpdateFace` (WL_AGENT.C) skips its `US_RndT()` calls while
`GETGATLINGSND` plays, so sound timing can move the random sequence after a
chaingun pickup (none of the four demos picks one up).

### Crash and correctness fixes found by running the game (ASan/UBSan)

| Where | Problem | Fix |
|---|---|---|
| `ID_US_1.C` `ParmStrings`, `ParmStrings2` | not nil-terminated; `US_CheckParm` read past them | nil added; `US_CheckParm` stops at the end of an all-symbol argument |
| `ID_US_1.C` `US_Print`, `US_CPrint` | wrote `'\0'` into the string argument; callers pass literals (read-only now) | lines are copied to a local buffer |
| `ID_US_1.C` `US_LineInput` | `pushf/cli` guard (not ported); overlapping `strcpy` | guard removed (events only arrive in `IN_PumpEvents`), `memmove` |
| `WL_MAIN.C` `FinishSignon` | `peekb(0xa000,0)` | `vl_vram[0]` |
| `port/wl_compat.c` `geninterrupt` | int 10h AX=3 (text mode before a quit message) aborted | ignored |
| `ID_VH.C` `VH_UpdateScreen`, `ID_VL.C` `VL_FadeIn` | 2D arrays indexed flat (UBSan bounds) | flat pointers |
| `WL_STATE.C` `CheckLine` | `(long)delta<<8` with negative delta (UB) | `*256` |
| `WL_DRAW.C` `TransformActor`, `TransformTile` | `viewx` widened to 64 bits by WOLF-4 | 32-bit wrapping multiply, stored as 16-bit like DOS `int` (`GunAttack` targets by `viewx`) |
| `WL_MENU.C` `Confirm` and other loops that poll `Keyboard[]` between `VW_UpdateScreen` calls | hung: the DOS keyboard ISR filled `Keyboard[]` on its own | `VH_UpdateScreen` pumps input; a key pressed and released between two polls keeps its break until the next pump, so polling loops see it |

### Config and save games

`port/wl_persist.[ch]`: files live in `$XDG_CONFIG_HOME/raylib_wolf3d/`
(`~/.config/raylib_wolf3d/`), are written field by field, little-endian,
with fixed sizes, and start with a magic and version (`RLW3DCFG`, `RLW3DSAV`,
version 1).

- **Config** (`ReadConfig`/`WriteConfig`, WL_MAIN.C): high scores, sound
  modes, input settings, scan codes, view size.  An invalid file is ignored
  (defaults), with a message on stderr.
- **Save games** (`SaveTheGame`/`LoadTheGame`, WL_MAIN.C): the 32-byte name
  written by the menu, then the game state, level ratios, `tilemap`,
  `actorat`, area tables, the actor list, statics, doors and pushwall, and an
  FNV-1a checksum.  Pointers are stored as indices: actor states into the
  table `wl_states[]` (generated from WL_AGENT.C/WL_ACT2.C by
  `scripts/gen_states.py` at build time, keeping their `#if` guards; append
  new states only), `actorat` actors by objlist slot (remapped to the slots
  the loaded actors get), `statobj.visspot` recomputed from the tile.
  Before anything is applied, `SaveLayoutValid` walks the whole body into
  throwaway variables: exact length, checksum, and range checks on every
  value that later indexes an array or picks a map, state or sprite (map and
  episode, weapons and frames, health, actor count/class/state/dir/tile/area,
  `actorat` slots, door numbers in `tilemap`, statics, doors, pushwall).  An
  invalid file is not listed in the load menu, and `LoadTheGame` rejects it
  with the original "corrupted" message without changing the game (the DOS
  version loaded it anyway and docked the player).
- Config scan codes must be `0..NumCodes-1` and mouse/joystick button
  mappings `bt_nobutton..NUMBUTTONS-1`, or the file is ignored.
- Tests: `-savetest` (save/load round trip of the state after demo 0 must
  give the same state hash; eight damaged saves with valid checksums must be
  rejected without changing the game) and malformed `CONFIG.WL1` files, both
  run by CTest `persist_check` (`tests/persist_check.py`).

### Demo regression check

`-demotest N|all [-viewsize N]` (`port/wl_demotest.c`) plays demos without
waiting for the 70 Hz clock (`PollControls`, and `ThreeDRefresh` skips its
retrace wait) and prints frames played, the end state and a hash of the game
state, actors, doors and tilemap.  id recorded each demo until the player
died, so a demo in sync uses all its input (`left=0`); exit 1 otherwise.
CTest `demo_sync` (`tests/demo_sync.cmake`) checks the recorded values.

All four shareware demos stay in sync at **view size 19**.  At the default
view size 15, demo 0 desyncs at frame 956: `GunAttack` only considers actors
within `shootdelta = viewwidth/10` pixels of `centerx`, and a guard that is
inside that window at width 240 is outside it at 304.  This is how the
original behaves too; the game's default stays 15.  The view size was found
by a frame-by-frame state comparison against Wolf4SDL built locally at
320x200 (not part of the repo); the other differences it showed were
Wolf4SDL's own changes (it marks the player's tile visible and grabs items
nearer than `MINDIST`) and changed nothing in the outcome.

`findfirst`/`findnext` (`port/wl_compat.c`) no longer keep a directory
handle between calls: `CheckForEpisodes` only tests for existence and
leaked it (found by LeakSanitizer in the `asan` preset).

### Still not ported

- The shareware order/end text screen on quit (`Quit` exits directly).
- `WL_DEBUG.C` debug keys that touch VGA memory (`MK_FP(0xa000,...)`) or
  `bioskey`; virtual-reality helmet input (`peek`, int 60h).

## Windows timer and file paths (WOLF-33)

`wl_timer.c` uses QueryPerformanceCounter, a CRT timer thread, and high-resolution
waitable timers on Windows 11. The POSIX monotonic-clock implementation remains
in use on Linux. Both advance the atomic 70 Hz TimeCount after game resets.

In PowerShell, pass `-data "C:\Games\Wolf shareware"`, or set
`$env:WOLF3D_DATA` to the external shareware v1.4 directory. Config and saves
use `%APPDATA%/raylib_wolf3d` (`LOCALAPPDATA` fallback), resolved before changing
the working directory to the assets. Linux retains its XDG config location.

The headless `core_platform` CTest needs no game data or display. To run it
independently with MinGW from the repository root:

```powershell
gcc -std=gnu99 -Iport -IWOLFSRC tests/platform_test.c port/wl_timer.c port/wl_platform.c port/wl_persist.c -o build-platform-test.exe
python tests/platform_check.py build-platform-test.exe
```

The harness supplies a fake window and empty asset placeholders; it tests
startup and paths, not game data validity or rendering.

## Non-portable C for Windows (WOLF-32)

Fixed in the shared sources, with the POSIX paths unchanged:

- `wl_compat.h`: `<strings.h>`/`<unistd.h>` only off MSVC (`<io.h>`, `ssize_t`,
  `_stricmp` there); `__attribute__((noreturn))` became C11 `_Noreturn`; `M_PI`
  is defined when missing; the DOS `cdecl`/`pascal`/`MAXINT`/`MAXLONG` macros
  are `#undef`ed first because the Windows headers predefine them.
- `findfirst`/`findnext` use `FindFirstFileA` on Windows (no `dirent`/`fnmatch`).
- `WL_Fsync` (`_commit`) and `WL_ReplaceFile` (`MoveFileExA`, replace existing)
  for the save-game temp-file-then-rename path: `rename()` fails on Windows when
  the target exists.
- `ID_MM.C` reads available RAM with `GlobalMemoryStatusEx` (no `sysconf`).
- `id_raylib.c`: GCC `__atomic_*` builtins became C11 `<stdatomic.h>`; the audio
  pacing clock goes through `RL_MonotonicNow` (MSVC has no `clock_gettime`).
  MSVC builds need `/std:c11 /experimental:c11atomics` (VS 2022 17.5+), now in
  `CMakeLists.txt`.
- Tests: `setenv`/`unsetenv` shims for `sd_integration_test.c`;
  `audio_ring_test.c` scripts time through `RL_MonotonicNow` (built with
  `RL_EXTERNAL_CLOCK`), because MinGW's `clock_gettime` is an inline function
  that cannot be replaced at link time, and its clock is `int64_t` (Windows
  `long` is 32 bits, so 2^31 ns = 2.1 s overflowed).

Checked and needing no change: `O_BINARY` is real on Windows and already used
for every `open()`; the `0600`/`0644` modes include the write bit so files
are not created read-only; data file names are upper-case `.WL1` and Windows
lookups are case-insensitive; config/save streams are explicit little-endian
(`wl_persist.c`) and Windows targets x86-64/ARM64 are little-endian; the game
`long` uses are 32-bit quantities, matching the DOS `long`.

Not covered here: `wl_timer.c`, `wl_platform.c`, `wl_persist.c` (WOLF-33; they
still include `pthread.h`/`unistd.h` unconditionally on master, so MSVC needs
that PR), the MSVC compiler flags (WOLF-31), and the tests `persist_test`
(`fork`/`sys/wait.h`) and `wolf_render_test` (relies on `--gc-sections`
dropping unreferenced functions; the Windows x64 `.pdata` sections keep them
alive and the link fails).

## WOLF-30 Windows audit

Audit of what blocks a native Windows 11 build (MSVC and MinGW-w64/Ninja).
Nothing here is fixed yet; each item names the file and a suggested fix.
Verified on Windows 11 with MSYS2 MinGW-w64 GCC 15.2, CMake, Ninja and a
system raylib 5.5: `cmake -B build -G Ninja -DBUILD_TESTING=ON`, then
`cmake --build build -- -k 0`. Configure succeeds; section A lists every
compile error that build reported. The main executable did not get as far as
linking, so section B is from reading the code. MSVC (`cl`) was not installed,
so MSVC remarks are expectations, not test results.

### A. Hard blockers (MinGW compile fails today)

| # | Where | Problem | Fix |
|---|-------|---------|-----|
| 1 | `WOLFSRC/ID_MM.C:114` | `sysconf(_SC_AVPHYS_PAGES / _SC_PAGESIZE)` is glibc/POSIX only (hits every target that compiles ID_MM.C). | `GlobalMemoryStatusEx` under `_WIN32`. |
| 2 | `port/wl_persist.c:19` | `mkdir(path, 0755)`: the Windows `mkdir` takes one argument. | Small wrapper using `_mkdir(path)` on `_WIN32`. |
| 3 | `port/wl_persist.c:29-35` | Config/save dir is `$XDG_CONFIG_HOME` (must start with `/`) or `$HOME/.config`. `HOME` is normally unset on Windows. | On `_WIN32` use `%APPDATA%\raylib_wolf3d`; accept drive-letter paths. Update `wl_persist.h` docs and `tests/persist_test.c`. |
| 4 | `port/wl_compat.c:6,95` | `<fnmatch.h>` / `fnmatch(FNM_CASEFOLD)` for `findfirst`/`findnext`; no such header on MinGW or MSVC. | `FindFirstFileA`/`FindNextFileA` under `_WIN32`, or a small `*`/`?` matcher. |
| 5 | `tests/persist_test.c:15` | `<sys/wait.h>`, `fork`/`waitpid`, `setenv`/`unsetenv`, `system("rm -rf ...")`, `mkdir(path, 0755)`. | Port (`_putenv_s`, in-process cases, CMake `-E rm -r`) or build only off Windows until ported. |
| 6 | `tests/audio_ring_test.c:11` | Defines its own `clock_gettime` to fake the clock; redefinition error against winpthreads, and it relies on symbol interposition. | Add a `WL_MonotonicNow()` hook used by `id_raylib.c`; the test supplies its own. |

### B. Expected link/runtime blockers (not reached yet)

- **70 Hz timer** (`port/wl_timer.c`): `pthread_create`, `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` and `nanosleep`. winpthreads has pthread and `clock_gettime`; MSVC has none. The default Windows timer tick is about 15.6 ms, coarser than the 14.3 ms game tick. Use `QueryPerformanceCounter` + `CreateThread` + a high-resolution waitable timer (or `timeBeginPeriod(1)` + `Sleep`) under `_WIN32`. `WL_Idle` (0.2 ms sleep) needs the same.
- **Other clock users**: `port/id_raylib.c:421,453` (audio pump), `audio/render.c:114`, `tests/sd_integration_test.c`: route through one clock abstraction.
- **Atomics**: `_Atomic longword TimeCount` (`WOLFSRC/ID_SD.C:15`) and the GCC `__atomic_*` builtins in `port/id_raylib.c` are fine on MinGW GCC. MSVC has no `__atomic_*`; use `<stdatomic.h>` with `/std:c11 /experimental:c11atomics`.
- **`chdir`** (`port/wl_platform.c:81`): `<unistd.h>` on MinGW, `_chdir` in `<direct.h>` on MSVC.
- **POSIX file API** (`ID_CA.C`, `ID_PM.C`, `wl_persist.c`, `wl_demotest.c`): `open/read/write/lseek/close`, `ssize_t`. MinGW provides them; MSVC needs `<io.h>` and the underscore names, and has no `ssize_t`.
- **`O_BINARY`**: `port/wl_compat.h:69` defaults it to 0 only if `<fcntl.h>` does not define it, which is correct for Windows. The game's own opens (`ID_CA.C`, `ID_PM.C`, `WL_MAIN.C` config, `WL_MENU.C` saves) already pass it; `port/wl_demotest.c` (`-savetest`), `tests/persist_test.c` and `CA_OpenDebug` do not and would run in text mode (CRLF translation, Ctrl-Z EOF) on Windows.
- **`port/wl_compat.h`** includes `<strings.h>`, `<unistd.h>`, `<sys/stat.h>` and maps `stricmp` to `strcasecmp`. MinGW is fine; MSVC has no `strings.h`/`unistd.h`, uses `_stricmp`, and lacks `S_IRUSR`/`S_IWUSR`/`S_ISREG`.
- **`__attribute__((noreturn))`** (`port/wl_compat.h:49`) is a GCC-ism; use `_Noreturn`/`__declspec(noreturn)` under `_MSC_VER`.
- **`long` is 32-bit on Win64** (LLP64) but 64-bit on Linux. `fixed` is already `int32_t`, and most game `long` fields were 32-bit under DOS, so those match better on Windows than on Linux. Risk is pointer-to-`long` casts: `CMakeLists.txt` suppresses `-Wint-conversion`/`incompatible-pointer-types` for GCC 14+, so run a `-Wpointer-to-int-cast` pass and move any pointer arithmetic to `intptr_t`.

### C. CMake and build system

- GCC-only flags are mostly guarded by `MATCHES "GNU|Clang"` but several test targets are not: `wolf_render_test` (`-ffunction-sections`, `-Wl,--gc-sections`), `wolf_core_data_test`, `wolf_codec_test`, `wolf_persist_test`, `wolf_sd_*` pass `-Wall -Wextra -fcommon ...` unconditionally and will fail under MSVC. The `-fsigned-char` pin has no direct MSVC flag (char is signed there by default, so it is already correct; `/J` would make it unsigned).
- Uppercase `.C` files are forced to C with `LANGUAGE C` (right for CMake with MSVC too); `-x c` is correctly GNU/Clang only.
- `CMAKE_C_STANDARD 99` is not what compiles: the build log shows `-std=gnu11` because `audio/` requests `c_std_11`. Set 11 at the top level and add `/experimental:c11atomics` for MSVC.
- `target_link_libraries(raylib_wolf3d PRIVATE ... Threads::Threads m)`: there is no `m` on MSVC; guard with `if(NOT MSVC)`. raylib supplies `opengl32/gdi32/winmm` itself.
- raylib comes from `find_package(raylib 5.5 EXACT)` (an MSYS2 package was found) with a FetchContent fallback that is untested on Windows. Decide static vs DLL; a MinGW release will want `-static` runtime linking so it does not need MSYS2 DLLs.
- `find_package(Python3 REQUIRED)` worked, but a clean Windows box often has only the `py` launcher or the Store stub. Document it, or commit the generated `wl_objdata.c`/`wl_states.c`. The `gen_states.py ... > file` redirect in `add_custom_command` works with Ninja but not portably with Visual Studio generators; give the script an `-o` option like `obj2c.py`.
- `WOLF3D_TEST_DATA` defaults to `$ENV{HOME}/wolf3d-data/shareware`; add a `USERPROFILE` fallback.
- `CMakePresets.json` only has Linux presets. Add `windows-mingw` (Ninja) and `windows-msvc` presets with `condition` on `hostSystemName`. ASan is MSVC-only (`/fsanitize=address`) and UBSan is unavailable there.

### D. Paths, names and line endings

- `WOLFSRC/ID_HEADS.H:17,19` include `"FOREIGN\JAPAN\GFXV_WJ1.H"` with backslashes. It sits in `#ifdef JAPAN` and is never compiled; switch to `/` if that build is ever enabled.
- Case: all includes match the on-disk names exactly (`WL_DEF.H`, `WOLF1VER.H`, lower-case `wl_compat.h`), so NTFS and Linux both work. Data files are looked up as `*.WL1` upper-case; Windows is case-insensitive but a Linux CI step is not.
- Line endings: `core.autocrlf` is `true` in this Git for Windows setup and the repo has no `.gitattributes`, so `WOLFSRC/` and shell scripts can be checked out as CRLF (breaking `#!/bin/bash` scripts) and binary files such as `WOLFSRC/OBJ/*.OBJ` rely on Git's heuristic. Add `.gitattributes` (`* text=auto`, `*.sh eol=lf`, `*.OBJ *.EXE *.WL1 *.png binary`).
- Path length: agent workdirs already exceed 150 characters; the CMake object paths under them are close to `MAX_PATH` (260) for MSVC. Build in a short directory (for example `C:\b\wolf`) or enable long paths.
- `DEICE.EXE`, `INSTALL.BAT` in the repo root are original DOS artefacts and confusing for Windows users; mention in the README.

### E. Scripts, tests and CI

- `scripts/*.sh` (including `fetch_shareware.sh`, `package.sh`, `xvfb_session.sh`) need bash, curl and (for the last) Xvfb: usable in Git Bash/MSYS2 except `xvfb_session.sh`; PowerShell equivalents are needed for a native flow.
- `tests/demo_sync.cmake` and `tests/persist_check.py` print `SKIPPED` without `xvfb-run`, so on Windows they never run. They redirect config with `XDG_CONFIG_HOME`; once the config dir is `%APPDATA%`, add a `WOLF3D_CONFIG_DIR` override and let both tests use it, and run them against the real window on Windows.
- `.github/workflows/build.yml` has only Ubuntu x86_64/arm64 jobs. Add a `windows-2022` MSYS2 MinGW64 job first (`mingw-w64-x86_64-{toolchain,cmake,ninja,raylib,python}`), MSVC later; leave the Ubuntu jobs untouched. The raylib-quarantine grep step works under `shell: bash`.
- `release.yml` and `scripts/package.sh` only produce Ubuntu artefacts; a Windows zip (exe, `raylib.dll` if shared, README, no game data) is a separate task.

### F. Checked and fine

- `raylib.h` is quarantined to `port/id_raylib.c`; raylib itself builds on Windows.
- `port/asm_stubs.c`, `port/wl_data.c`, `port/wl_raycast.c`, `port/wl_timer.c` (compiles; runtime behaviour per B), `port/id_raylib.c`, `audio/sd_audio.c` and `audio/vendor/opl3.c` compiled without errors under MinGW GCC 15.
- No `fork`, `mmap`, signals, sockets, `dlopen`, `alloca` or VLAs in the game path; the inline asm is behind `#if 0` or in files that are not built (`CONTIGSC.C`, `OLDSCALE.C`, `DETECT.C`).
- Structures are read field by field, so no packing/endianness change is needed on Win64.

### Suggested order

1. Add small `port/` shims (clock/sleep, mkdir, config dir, `findfirst`, memory size) and fix A1-A6 so MinGW builds, links and runs.
2. Add the CMake presets, `.gitattributes` and a MinGW CI job.
3. Run `-demotest all -viewsize 19` on Windows against the recorded hashes.
4. MSVC: flags, atomics, `strings.h`/`unistd.h` replacements.
5. Windows packaging.

## WOLF-49 macOS audit

Audit of what blocks a native macOS arm64 build (AppleClang, macOS 13+,
raylib 5.5). Nothing is fixed here; each item names the file, a suggested fix
and the macOS issue that owns it (macOS 2 = WOLF-50 toolchain, 3 = WOLF-51
portable C, 4 = WOLF-52 platform layer, 5 = WOLF-53 raylib backend, 6 =
WOLF-54 audio, 7 = WOLF-55 ctest + CI, 8 = WOLF-56 `.app` + README).

Host: Apple Silicon (arm64), macOS 26.6.2, Apple clang 21.0.0, CMake 3.29.2,
Ninja 1.12.0, Homebrew. Master `b1d3d52`:

    cmake --preset release -DWOLF3D_USE_SYSTEM_RAYLIB=OFF
    cmake --build build -- -k 0

Configure succeeds. The build fails on the three compile errors in section A
and the `--gc-sections` link error in section B. With those patched locally
(not committed),
the game links, `ctest` passes 8/8 (`demo_sync` and `persist_check` skip:
no `xvfb-run`), the four shareware demos give the Linux hashes when run
directly (`raylib_wolf3d -demotest all -viewsize 19`, also under the `asan`
preset with no ASan/UBSan reports), the title screen renders on a Retina
display, and `wolf_sd_device` plays through miniaudio's Core Audio backend.

### A. Hard blockers (AppleClang compile fails today)

| # | Where | Problem | Fix | Owner |
|---|-------|---------|-----|-------|
| 1 | `WOLFSRC/ID_MM.C:127` | `sysconf(_SC_AVPHYS_PAGES)` does not exist on Darwin (5 targets fail). The Windows port (WOLF-32) put it in the non-`_WIN32` branch, so it still fails there. | `host_statistics64(HOST_VM_INFO64)` free + inactive pages, or `sysctl hw.memsize`, under `__APPLE__`; keep the 64 MB fallback. | 3 |
| 2 | `port/wl_timer.c:178` | `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` does not exist on Darwin (implicit declaration is an error in clang 16+). | Under `__APPLE__`: compute the remaining time against `clock_gettime(CLOCK_MONOTONIC)` and `nanosleep` it in a loop (or `mach_wait_until`). Must not drift: the 70 Hz tick is absolute. | 4 |
| 3 | `audio/render.c:149` | Same `clock_nanosleep(TIMER_ABSTIME)` in the `--realtime` pacing of `wolf_audio_render`. | Share the helper from item 2, or a relative `nanosleep` loop. | 6 |

### B. Toolchain and link (CMake)

- **`-Wl,--gc-sections`** (`wolf_render_test`): Apple ld says `unknown
  options: --gc-sections` at `b1d3d52`. Master now sets `WOLF3D_GC_LINK`
  only `NOT APPLE` (`CMakeLists.txt:138-141`), so this no longer fails; set
  it to `-Wl,-dead_strip` on `APPLE`. (macOS 2)
- **Deployment target**: nothing sets `CMAKE_OSX_DEPLOYMENT_TARGET`, so the
  binary gets `minos 26.0` (the SDK) and will not start on macOS 13-25. With
  `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0` it gets `minos 13.0` and builds without
  availability warnings. Set it to 13.0 before `project()` (cache default).
  (macOS 2)
- **Homebrew raylib is 6.0**, not 5.5. `find_package(raylib 5.5 EXACT)`
  rejects it quietly and the build falls back to FetchContent (checked with
  the 6.0 bottle on `CMAKE_PREFIX_PATH`). That is the right outcome, but the
  "Homebrew raylib 5.5" path does not exist: document FetchContent as the
  macOS path (needs network on first configure) and print why the system
  raylib was skipped. `WOLF3D_RAYLIB_VERSION` is a plain variable, not a
  cache option. (macOS 2)
- **Presets**: no macOS preset. A `macos` configure/build/test preset with
  Ninja, `CMAKE_OSX_ARCHITECTURES=arm64` and the deployment target keeps the
  CI and README commands short. (macOS 2)
- **Linker warning**: `ld: warning: reducing alignment of section
  __DATA,__common from 0x8000 to 0x4000` when linking `raylib_wolf3d` and
  `wolf_sd_device` (large `-fcommon` globals). Harmless; quiet it by
  defining the large globals once instead of `-fcommon`, or leave it. (macOS 3)
- raylib configure prints a CMake dev warning that OpenGL is deprecated since
  macOS 10.14. raylib 5.5 still gets a working OpenGL 3.3 core context. No
  action. (macOS 2)
- Frameworks: raylib's link interface brings in OpenGL, Cocoa, IOKit,
  CoreFoundation, CoreGraphics, CoreServices, AppKit and Foundation (`otool
  -L`); `m` and `Threads::Threads` resolve to libSystem. No change needed. (macOS 2)
- The GCC 14 `-Wno-error=...` list is GNU-only. AppleClang 21 is fine
  without it: the only errors are the implicit `clock_nanosleep` declarations
  in A. (macOS 2)

### C. Warnings (AppleClang `-Wall`, default filtering)

36 distinct warnings in project sources (CI's GCC build does not report these classes). None is an error and none
changes the demo hashes; fix or silence in macOS 3 (game) and macOS 6 (audio).

- `-Wabsolute-value` (12): `abs()` of `unsigned` tile differences in
  `WL_ACT2.C` (2387..3458) and `WL_AGENT.C:1227-1228`. The result is right
  (the wrapped value converts back to a negative `int`); cast to `int` first.
- `-Wimplicit-enum-enum-cast` (9): `boolean` stored in `activetype`
  (`WL_ACT2.C`, `WL_AGENT.C:1103`, `WL_DRAW.C:951`, `WL_PLAY.C:941`) and
  `SMMode` -> `SDMode` (`WL_MAIN.C:190`). Original code; add
  `-Wno-implicit-enum-enum-cast` to the filtered list.
- `-Wchar-subscripts` (4): `WL_MENU.C:2341,2747,3806`, `WL_INTER.C:384`. char
  is signed on Apple arm64 (and the build pins `-fsigned-char` anyway), so
  check each index is never negative.
- `-Wsometimes-uninitialized`: `ID_IN.C:619` (`realdelta` after
  `Quit("Demo playback exceeded")`; false positive because `Quit` is not
  declared `noreturn` (`WL_DEF.H:800`, `ID_HEADS.H:86`)) and `audio/render.c:20` (`size` after a
  failed `fseek`; `fail()` exits, so also false, but mark `fail` noreturn).
- `-Wdangling-else`: `WL_MENU.C:1653`.
- `-Wstatic-in-inline` (8): `audio/vendor/opl3.c` (vendored Nuked OPL3).
  Silence per target; do not edit the vendored file.

### D. Platform layer (paths, data, timer)

- **Config and saves** (`port/wl_persist.c:60`): `$XDG_CONFIG_HOME` or
  `~/.config/raylib_wolf3d`. Works on macOS, but the macOS place is
  `~/Library/Application Support/raylib_wolf3d`. Use that under `__APPLE__`
  when `XDG_CONFIG_HOME` is unset (keep `XDG_CONFIG_HOME` for the tests);
  update `wl_persist.h` and `tests/persist_test.c`. (macOS 4)
- **Data lookup** (`port/wl_platform.c`): `-data`, `$WOLF3D_DATA`, then the
  current directory. A Finder-launched `.app` starts with cwd `/`, so add
  `~/Library/Application Support/raylib_wolf3d/data` (or the bundle's
  `Contents/Resources`) as a default. stderr is invisible from Finder, so
  the missing-data message also needs a visible form (e.g. a log file). (macOS 4, macOS 8)
- The game `chdir`s into the data directory, so relative output (e.g. a
  raylib `TakeScreenshot`) lands there. No change needed.
- APFS is case-insensitive by default; `findfirst` uses
  `fnmatch(FNM_CASEFOLD)` (`port/wl_compat.c`), which Darwin's libc has. OK.
- `_GNU_SOURCE` in `wl_compat.c`/`wl_timer.c` is harmless on Darwin.
- Atomics (`_Atomic`, `__atomic_*`), `pthread`, `clock_gettime
  (CLOCK_MONOTONIC)`, `nanosleep`: all present on macOS 13+. OK.
- No `/proc`, `endian.h`, `byteswap.h`, `open_memstream`/`fmemopen` or
  `memfd` users in `port/`, `audio/` or `WOLFSRC/`. On-disk data is read
  byte-wise (`WL_ReadLE16/32`), no packed structs: no arm64 alignment issue
  (ASan/UBSan clean on the demos).

### E. raylib backend (Retina, fullscreen, keys, mouse)

- **Retina**: GLFW turns on the Retina framebuffer by default, with or
  without `FLAG_WINDOW_HIGHDPI`. Measured on a 2x display: screen 960x720
  points, render 1920x1440 pixels, scale 2.0 in both modes, and the frame
  fills the window. `EnsureBigTarget` sizes the intermediate texture from
  `GetScreenWidth()` (points), so it upscales 3x then the GPU stretches 2x
  (slightly soft). Size it from `GetRenderWidth()` on Apple. Decide whether
  to set `FLAG_WINDOW_HIGHDPI` explicitly (also changes the window size on
  Windows/X11, so keep it Apple-only). (macOS 5)
- **Fullscreen**: Alt+Enter (`id_raylib.c:166-178`) is Option+Return on a
  Mac keyboard and calls `ToggleBorderlessWindowed`. Mac users expect
  Ctrl+Cmd+F / the green button (native Spaces fullscreen). Add Cmd+F or
  Ctrl+Cmd+F, keep Option+Return. (macOS 5)
- **Cmd keys**: `KEY_LEFT_SUPER`/`KEY_RIGHT_SUPER` are not in the scancode
  table, so Cmd is ignored by the game, which is right. GLFW's default
  menu gives Cmd+Q -> window close -> `quit_requested`; Cmd+H/Cmd+M are
  handled by AppKit. Make sure Cmd+letter shortcuts are not also delivered to
  the game as letters; verify by hand. (macOS 5)
- **Mouse capture**: `DisableCursor` on Cocoa hides and pins the cursor
  (no raw motion on macOS). Needs a manual check that menu mouse and in-game
  turning feel right on a mouse and a trackpad. (macOS 5)
- `SetExitKey(KEY_NULL)` and the window-close quit path are fine.

### F. Audio

- raylib 5.5's miniaudio picks Core Audio: `Backend: miniaudio | Core Audio`,
  48000 Hz stereo, period 1440 frames. `wolf_sd_device` played music, AdLib,
  PC speaker and digitized sounds; `sound_manager`, `audio_ring`,
  `audio_contract` pass. Only A3 and the `opl3.c` warnings remain. (macOS 6)
- `audio/README.md` says realtime pacing uses `clock_nanosleep` "on Ubuntu"
  and shows `xvfb-run`; add the macOS command lines. (macOS 6)

### G. Scripts, tests and CI

- **`scripts/fetch_shareware.sh:40`**: `realpath -m` fails on macOS
  (`realpath: illegal option -- m`). Create the directory first and use plain
  `realpath`, or `cd "$dest" && pwd -P`. Rest of the script works with the
  stock bash 3.2 (no bash 4 features), `curl` and `unzip`. `sha256sum` is in
  `/sbin` on this macOS 26 host but missing on older releases; fall back to
  `shasum -a 256`. With the `realpath` change all 8 WL1 files PASS. (macOS 7)
- **`demo_sync` / `persist_check`** skip on macOS because they require
  `xvfb-run`. A Mac (and the GitHub macOS runners) has a window server: run
  the game directly when `APPLE` (no Xvfb), keeping the scratch
  `XDG_CONFIG_HOME`. The demo hashes already match. (macOS 7)
- **No `timeout`/`gtimeout`** in a stock macOS: scripts and docs must not
  depend on it (CTest `TIMEOUT` is fine). (macOS 7)
- **CI**: `.github/workflows/build.yml` has Ubuntu x86_64/arm64 jobs only.
  Add a `macos-15` (arm64) job: Ninja from Homebrew, FetchContent raylib
  (cache `_deps`), configure with deployment target 13.0, build, `ctest`,
  and the no-data smoke test (`exit 2`). Not `macos-14`: that image has
  brownouts and is retired on November 2, 2026; the 13.0 deployment target
  keeps macOS 13 support whatever the runner runs. The fake-data smoke test
  opens a window; this should work on the runners without Xvfb (not tested
  yet). Keep the raylib quarantine check. (macOS 7)
- **Packaging**: `scripts/package.sh` is Linux-only (GNU `tar --sort
  --owner --mtime`, `touch -d @`, `dpkg-deb`, `dpkg-shlibdeps`). The `.app`
  bundle needs its own script (`Info.plist`, icon, `codesign -s -`, a
  tarball or zip with `ditto`). Local builds are ad-hoc signed by ld and run
  as-is; a downloaded `.app` needs the quarantine flag explained in the
  README. (macOS 8)
- Screen capture from an agent session needs Screen Recording permission
  (`screencapture` fails with "could not create image from display"). Use an
  in-game frame dump for evidence. (macOS 7)

### Suggested order

1. macOS 2: deployment target, `-dead_strip`, `macos` preset, raylib
   message. Rebase on the Windows CMake PRs (#16, #23) first.
2. macOS 3 and 4: A1, A2 and the warnings, as `__APPLE__` branches next to
   the `_WIN32` ones from the Windows PRs.
3. macOS 6: A3.
4. macOS 7: `fetch_shareware.sh`, Xvfb-free demo/persist tests, CI job.
5. macOS 5: Retina target size, Cmd+F fullscreen, manual input checks.
6. macOS 8: `.app` bundle and README.
