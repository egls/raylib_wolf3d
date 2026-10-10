# Standalone Wolf3D audio

`wolf_audio` is a plain C11 pull mixer with no raylib/SDL/device headers. It
produces interleaved signed 16-bit stereo, combining Nuked OPL3 in OPL2 mode,
IMF music, one AdLib **or** PC effect voice, and one digitized PCM voice.
The Stage 3 backend can call `sd_audio_mix` from `id_raylib.c`; all calls on an
engine must be serialized. For an audio callback, send commands to its owning
thread or use a game-thread mixer and an SPSC sample ring. Input buffers are
borrowed until stopped/replaced; callers must keep them alive. The mixer does
not allocate memory, read files, or sleep. Creation/destruction are outside the
callback. This issue does not wire the DOS `ID_SD` manager or raylib output yet.

## Build and run

The foundation CMake project has not landed on `master` at this change's base
(`05167784ef009d0d0daefe8d012b027f39dc8541`). Build this directory alone:

```sh
cmake -S audio -B build-audio -DCMAKE_BUILD_TYPE=Debug
cmake --build build-audio -j2
ctest --test-dir build-audio --output-on-failure
python3 audio/tests/fetch_shareware.py
python3 audio/tests/fetch_shareware.py --check-only
build-audio/wolf_audio_render "$HOME/wolf3d-data/shareware" music 0 12 corner.wav --realtime
build-audio/wolf_audio_render "$HOME/wolf3d-data/shareware" adlib 24 2 pistol.wav --realtime
build-audio/wolf_audio_render "$HOME/wolf3d-data/shareware" pcm 0 2 digi.wav --realtime
build-audio/wolf_audio_render "$HOME/wolf3d-data/shareware" pc 24 2 pc.wav --realtime
```

The foundation can use `add_subdirectory(audio)` and link `wolf_audio` when it
lands. Core code is portable C; this headless harness's optional wall-clock
pacing uses POSIX `clock_nanosleep` on Ubuntu. `--rate HZ` accepts 700..384000 Hz
(560 minimum with Spear). `--trace OUT.csv` writes frame/register/value events.
WAV output is explicitly little-endian PCM, independent of host byte order.
Without `--realtime`, rendering uses only the sample clock and runs offline;
all attached evidence was generated with `--realtime` and absolute monotonic
clock deadlines. A null audio sink cannot accelerate these recordings.

For the 560 Hz sequencer (no SOD data needed):

```sh
cmake -S audio -B build-audio-spear -DSD_AUDIO_SPEAR=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-audio-spear -j2
ctest --test-dir build-audio-spear --output-on-failure
```

## Shareware data and on-disk layouts

`fetch_shareware.py` uses the [Wolf3D.net v1.4 shareware repack](https://beta.wolf3d.net/games/29/downloads/4477),
verifies archive SHA-256
`3ff0798f7d912dcd03f78419b55d63dd1934a93b6b3be1fab425dc2b148612cc`, and extracts
only `AUDIOHED.WL1`, `AUDIOT.WL1`, and `VSWAP.WL1`. It validates the individual
SHA-256 values pinned in that script before writing and refuses a cache inside
the repository. It neither extracts executables nor uses commercial data.

The v1.4 shareware AUDIOHED has 289 offsets: 87 PC, 87 AdLib, 87 digitized
placeholder slots, and 27 music chunks. `WOLFSRC/AUDIOWL1.H` describes earlier
69-slot data, also recognized by the harness; its constants must not be used
for the v1.4 data. SoundCommon is a 4-byte LE length plus 2-byte LE priority;
AdLib adds the 16-byte instrument, one block byte, then frequency bytes. Parsing
uses bytes rather than packed native structs. PCM indexes refer directly to
the VSWAP final sound-info table, not logical game SFX numbers. The harness
joins VSWAP pages according to the original 16-bit sound length; the module
accepts the resulting contiguous unsigned 8-bit 7042 Hz buffer. V1.4 shareware
sounds fit this original length format. Extended modded VSWAP sizes are outside
this harness's scope. Sparse or truncated sound pages fail explicitly.

## Timing and reference evidence

Sequencing is based on the repository's `WOLFSRC/ID_SD.C` and
`WOLFSRC/ID_SD_A.ASM` (`SDL_t0FastAsmService`, `DOFX`). Reference behavior was
read and tested against [Wolf4SDL](https://github.com/11001011101001011/Wolf4SDL)
at `dc8b250af35fb0ace68db5eb879490b50068c20e`, `id_sd.c` functions
`SDL_IMFMusicPlayer`, `SDL_ALPlaySound`, `SDL_AlSetFXInst`,
`SDL_PCMixCallback`, and `GetSample`/`SD_PrepareSound`.

* Wolf IMF ticks at 700 Hz; Spear at 560 Hz. Rational sample clocks prevent
  integer division drift at 48 kHz or other non-divisible sample rates.
* AdLib effects update at 140 Hz, after every five Wolf music ticks (four with
  Spear). Channel 0 uses operators 0/3, ignores `nConn`, treats a zero frequency
  byte as key-off, and keys off immediately on consuming the last byte.
* IMF zero-delay writes share a tick. Like the original ISR and Wolf4SDL,
  looping restarts on the tick following the last write, ignoring the last
  event's delay. All-zero-delay loops execute at most one pass per tick.
  Non-loop playback leaves final notes ringing; explicit stop keys off music
  channels 1..8 and percussion while preserving the effect on channel 0.
* PC sound bytes run at 140 Hz. The square wave uses PIT frequency
  `1193180 / (byte * 60)`, silence for zero, and Wolf4SDL's amplitude 8192.
* PCM uses Wolf4SDL's clipped three-point quadratic interpolation and its
  integer destination length. Stereo positioning uses the original 0..15
  attenuation scale, 0 full volume and 15 muted. Its priority is independent
  of the synthesized effect. Lower priority replacements are rejected.

Reproduce the controlled reference test (Python 3, Git and a C compiler):

```sh
git clone https://github.com/11001011101001011/Wolf4SDL.git "$HOME/wolf3d-data/Wolf4SDL"
git -C "$HOME/wolf3d-data/Wolf4SDL" checkout dc8b250af35fb0ace68db5eb879490b50068c20e
python3 audio/tests/compare_reference.py "$HOME/wolf3d-data/Wolf4SDL" "$HOME/wolf3d-data/shareware" build-audio/wolf_audio_render
```

The test extracts the **actual** pinned reference functions into a temporary
program and supplies a Nuked output adapter. It compares every PCM sample for
music 0 (12 seconds), AdLib 0/24/26, PC 0/24, and digitized 0/1/2; IMF/AdLib
also compare every register write and frame timestamp. All nine cases passed
at 44100 Hz, where reference tick divisors are exact. This establishes timing,
layout, synthesis commands, and resampling equivalence; it does not compare
Nuked timbre against Wolf4SDL's different MAME/DBOPL emulator or physical OPL2
hardware, and it is not a full game/device capture. Arbitrary-rate clock and
buffer partition invariance are tested separately in `audio_contract`.

Sanitizer validation:

```sh
cmake -S audio -B build-audio-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-audio-sanitize -j2
ctest --test-dir build-audio-sanitize --output-on-failure
python3 audio/tests/compare_reference.py "$HOME/wolf3d-data/Wolf4SDL" "$HOME/wolf3d-data/shareware" build-audio-sanitize/wolf_audio_render
```

## Provenance and licensing

Nuked OPL3 1.8 source/header and license are copied **unmodified** from
[raylib_DOOM](https://github.com/raydelto/raylib_DOOM/tree/44f6029699ee715565aa0b378c014fb44ab36f89/linuxdoom-1.10)
at `44f6029699ee715565aa0b378c014fb44ab36f89`. File SHA-256:

```text
59eb873fdb6d52bc7977a0fcbb97c1bae03dd71cc88e3acddbcc01b7121bfe03  vendor/opl3.c
a84266b8d71a4929f15f573afbe407fd24310b77757d855835dacabf68763679  vendor/opl3.h
20c17d8b8c48a600800dfd14f95d5cb9ff47066a9641ddeab48dc54aec96e331  vendor/LICENSE
```

Nuked is Copyright 2013–2020 Nuke.YKT, **LGPL-2.1-or-later** (full text in
`vendor/LICENSE`). The new audio code and harness/tests are
**GPL-2.0-or-later**, with full text in `audio/LICENSE`; this does not change the
license of other repository files. Credit Jason Blochowiak/id Software,
Wolf4SDL contributors, and Chocolate Doom's PC speaker mixer. Wolf4SDL's
README explicitly offers the original engine under GPL v2 as an alternative
to the id limited-use license. No Wolf4SDL source is vendored here. When
redistributing a linked binary, provide corresponding audio/Nuked source and
build instructions so the library can be modified and the executable rebuilt.
