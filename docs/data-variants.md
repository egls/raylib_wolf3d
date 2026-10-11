# WL1 and WL6 data variants

WOLF-44 uses two binaries, rather than runtime selection inside the engine.
The original `UPLOAD` and `GOODTIMES` branches affect menus, episodes, articles,
graphics indices and ending screens. Compiling those paths separately keeps
WL1's existing behaviour and demo hashes, and avoids remapping every resource.
Only the platform backend and audio library are shared.

Both variants are built in one configuration:

```sh
cmake --preset release
cmake --build --preset release -j 4
ctest --preset release
source ~/emsdk/emsdk_env.sh
cmake --preset web
cmake --build --preset web -j 4
```

| Data | Native target/executable | Web artifacts |
| --- | --- | --- |
| Shareware v1.4 WL1 | `raylib_wolf3d` (existing name retained) | `raylib_wolf3d.html/.js/.wasm/.data` |
| Registered WL6 | `wolf3d-wl6` | `wolf3d-wl6.html/.js/.wasm` (no data package) |

WL6 uses the original `VERSION.H` (`GOODTIMES`, no `UPLOAD`),
`GFXV_WL6.H`, `AUDIOWL6.H` and `MAPSWL6.H`. This supports the registered
v1.4 **149-chunk / 132-picture layout provided by the original source**.
It requires 60 maps and 288 audio chunks, including 27 music tracks.
Different registered distributions with another graphics layout are rejected;
the extension alone does not establish compatibility. WL3, SOD and SDM are
not supported by this variant. Actual registered gameplay remains untested.

The cache/page managers retain explicit little-endian reads and bounded file
ranges. Startup checks graphics/audio table sizes and monotonic offsets, map
headers and plane ranges, and all 60 registered map entries. Decompression,
picture sizes and font glyph ranges are checked when loaded. Diagnostics identify
the compiled extension. `-datacheck` checks all available maps, graphics and audio,
and the VSWAP directory without opening a window, playing sound, or writing saves:

```sh
./build/wolf3d-wl6 -data /path/to/your/own/data -datacheck -nosound
./build/wolf3d-wl6 -data /path/to/your/own/data
```

## Contract for the WOLF-43 web shell

`build-web/variants.json` maps uppercase extensions to relative script and wasm
filenames. Keep these names stable. WL1's `.data` exists only when shareware was
bundled; WL6 never preloads registered data. The generated WL6 HTML still uses the
Stage 1 shareware shell; it is not the registered-data picker. The WOLF-43 shell
must choose the script before instantiating a fresh Emscripten runtime:

1. Validate the eight required basenames, normalize names to uppercase, and reject
   mixed extensions. Select `variants[extension].script` (WL1 or WL6 only).
2. Set `Module` before loading that script. In `preRun`, write the selected files
   into MEMFS at `/wolf3d/data/NAME.WL6` (or `.WL1`), and configure a distinct
   `XDG_CONFIG_HOME` for that data set. Preserve the existing canvas and handlers.
3. After runtime initialization and the start gesture, call
   `Module.callMain(['-data', '/wolf3d/data'])`. Keep `printErr`, `onExit` and
   `onAbort` wired to visible errors. Switching variants requires a new runtime
   (page reload is simplest); never call main twice after exit.

No registered data is committed, fetched, bundled, or sent to a server.

## Agent checks (synthetic data only)

`synthetic_wl6` derives counts from a C probe including WOLFSRC headers, generates
an inert Huffman dictionary, picture/font chunks, 60 uniform maps, silent audio
and a minimal VSWAP, then runs the actual native executable's `-datacheck`.
It also tests truncated/mismatched tables and invalid offsets. These fixtures
are not playable game assets and contain no registered data.

For a browser check of the same loader, use an installed Playwright with Chromium:

```sh
python3 -I tests/synthetic_wl6.py build/wolf3d-wl6 build/wolf_wl6_layout \
  build/raylib_wolf3d build/synthetic-web-data
PLAYWRIGHT=/path/to/node_modules/playwright/index.mjs \
  node tests/synthetic_wl6_web.mjs build-web build/synthetic-web-data
```

The browser check serves locally, loads the synthetic set into MEMFS, checks a
successful structural run and a graceful mismatched-header rejection, then closes
its browser and server. It tests no real registered gameplay or registered audio.

## Raydelto's five-step gameplay checklist

1. Use your own registered v1.4 WL6 copy. Load all eight files in the finished web
   shell; confirm WL6 is selected and startup succeeds. If rejected, record the
   diagnostic and `VGAHEAD.WL6` size (supported layout: 450 bytes).
2. Check title, menus and all six episode choices. Start E1M1 and E6M1; verify
   pictures/text, movement, doors, combat, weapons and pickups.
3. Enable AdLib music and digitized sound; check title music, weapon/enemy effects,
   sound settings and a later-episode music track.
4. Save and reload in two different episodes, then reload the browser and load
   again. Confirm WL6 settings/saves stay separate from the default WL1 set.
5. Finish a level and an episode (including their intermission/ending screens),
   then switch back to bundled WL1 and play E1M1. Report browser/version, data
   edition, selected build and any diagnostic; do not upload registered files.
