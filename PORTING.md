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

### Temporary silent Sound Manager

`port/sd_silent.c` is built **instead of** `WOLFSRC/ID_SD.C` while
`WOLF3D_SILENT_SD` is ON (the default).  It is a machine with a PC speaker
and nothing else, unplugged: modes are accepted and remembered, nothing plays,
`SD_SoundPlaying()` is always 0.  Delete it and the CMake option when WOLF-6
is merged.  Known coupling to keep in mind: `UpdateFace` (WL_AGENT.C) skips
its `US_RndT()` calls while `GETGATLINGSND` plays, so sound timing can move
the random sequence after a chaingun pickup (none of the four demos picks one
up).

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
  `LoadTheGame` verifies the whole file first and changes nothing if it's
  invalid (the DOS version loaded it anyway and docked the player).  The load
  menu only lists files in this format.

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

### Still not ported

- The shareware order/end text screen on quit (`Quit` exits directly).
- `WL_DEBUG.C` debug keys that touch VGA memory (`MK_FP(0xa000,...)`) or
  `bioskey`; virtual-reality helmet input (`peek`, int 60h).
