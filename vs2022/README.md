# Visual Studio 2022 project

`raylib_wolf3d.sln` builds the shareware (WL1) game for x64 Debug/Release with
MSVC (v143, VS 2022 17.5+ for C11 atomics). Output: `build-vs\x64\<config>\raylib_wolf3d.exe`.

- raylib 5.5 comes from the vcpkg manifest `vcpkg.json` (pinned via baseline + override; triplet `x64-windows-static-md`; needs a full-history vcpkg clone).
  Either run `vcpkg integrate install` once, or set `VCPKG_ROOT` to a vcpkg checkout
  (a VS 2022 Developer Command Prompt may already set it).
- Python 3 must be on `PATH` (or pass `/p:WolfPython=...`): it generates `wl_objdata.c`
  and `wl_states.c` from `WOLFSRC/OBJ` and the sources, like the CMake build.
- Game data is not part of the repository; see the main README (`-data <dir>`).

```
msbuild vs2022\raylib_wolf3d.sln /p:Configuration=Release /p:Platform=x64
```
