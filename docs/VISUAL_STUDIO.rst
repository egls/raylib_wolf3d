Building with Visual Studio
===========================

This page covers building and debugging the shareware (WL1) game from the
Visual Studio IDE (Debug and Release, x64).  The CMake/MinGW and CMake/MSVC
command-line builds in ``README.rst`` are unaffected.

.. note::

   The solution and project files are added by the "Visual Studio port"
   series (WOLF-99).  File locations below are written as ``<vs-dir>`` for the
   directory that holds ``raylib_wolf3d.sln``; use the path your checkout has.

Prerequisites
-------------

* Windows 10/11 x64.
* Visual Studio 2022 (17.5 or newer) with the **Desktop development with C++**
  workload (MSVC v143 toolset and a Windows 10/11 SDK).  Newer Visual Studio
  releases work if they keep the v143 toolset installed.
* Git.
* Python 3 on ``PATH`` (``python --version`` in a Developer PowerShell).  The
  build runs the same generator scripts as CMake to create ``wl_objdata.c`` and
  ``wl_states.c``.
* vcpkg, for raylib 5.5 (next section).  Use a full-history clone, not a
  shallow one, so the pinned baseline can be resolved.
* The shareware v1.4 data (``*.WL1``).  It is not in the repository; see
  "Game data" below.  Only WL1 data is supported, not WL6 or SOD.

raylib through vcpkg
--------------------

raylib 5.5 comes from the vcpkg manifest (``vcpkg.json``) that sits next to the
solution; the version is pinned and no absolute paths are involved.  Set vcpkg
up once, from PowerShell, in any directory outside this repository::

    git clone https://github.com/microsoft/vcpkg.git
    .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
    .\vcpkg\vcpkg.exe integrate install

``integrate install`` makes Visual Studio and MSBuild restore the manifest
automatically on the first build (user-wide, once per machine).  As an
alternative, set ``VCPKG_ROOT`` to the vcpkg checkout instead; a Developer
Command Prompt may already define it when vcpkg was installed with Visual
Studio's "vcpkg package manager" component.

The first build compiles raylib, so it takes a few minutes.  Later builds
reuse it.

Opening and building
--------------------

1. Open ``<vs-dir>\raylib_wolf3d.sln`` (File > Open > Project/Solution).
2. Pick the **x64** platform and **Debug** or **Release** in the toolbar.
   There is no Win32 configuration.
3. Build > Build Solution (``Ctrl+Shift+B``).

The same from a Developer PowerShell for VS::

    msbuild <vs-dir>\raylib_wolf3d.sln /p:Configuration=Release /p:Platform=x64

The executable is ``raylib_wolf3d.exe`` in the configuration's output folder
(``...\x64\Debug`` or ``...\x64\Release``).  The output folders are ignored by
git.

Game data
---------

Fetch and checksum-verify the shareware data once; it lands outside the
repository::

    scripts\fetch_shareware.ps1        # -> %USERPROFILE%\wolf3d-data\shareware

If scripts are blocked, run ``powershell -ExecutionPolicy Bypass -File
scripts\fetch_shareware.ps1``.

Running from Visual Studio
--------------------------

Visual Studio starts the debuggee with the *project* directory as the working
directory, not the folder holding the exe, so a relative data lookup fails
unless the project overrides it.  The project configures this for you: F5 /
``Ctrl+F5`` runs the game with the data directory taken from ``WOLF3D_DATA``,
falling back to ``%USERPROFILE%\wolf3d-data\shareware``.  With the data fetched
as above nothing else is needed.

To use another data directory:

* set the ``WOLF3D_DATA`` environment variable (restart Visual Studio so it
  sees it), or
* set Project > Properties > Debugging > **Command Arguments** to
  ``-data C:\path\to\wl1-data`` (use your own path; do not commit it), or
* copy the ``*.WL1`` files next to the exe.

Useful Command Arguments while debugging: ``-nosound`` (mute),
``-datacheck`` (verify the data and exit).  Set breakpoints as usual; the game
logic is plain C in ``WOLFSRC`` and ``port``.

Troubleshooting
---------------

``raylib.h`` / ``raylib.lib`` not found, or "Cannot find vcpkg"
    vcpkg is not integrated.  Run ``vcpkg integrate install`` or set
    ``VCPKG_ROOT``, then rebuild.  Check the Output window (Show output from:
    Build) for the vcpkg manifest install log.

vcpkg cannot resolve the baseline / "failed to checkout"
    The vcpkg clone is shallow.  Run ``git fetch --unshallow`` in it.

``python`` is not recognized, or ``wl_objdata.c`` / ``wl_states.c`` missing
    Python 3 is not on ``PATH`` for Visual Studio.  Install Python 3, restart
    Visual Studio, or pass ``/p:WolfPython=<path to python.exe>`` to MSBuild.

``error MSB8020`` / platform toolset v143 not found
    Install the **MSVC v143** build tools and a Windows SDK through the Visual
    Studio Installer, or retarget the project (Project > Retarget Solution).

Linker error about ``CloseWindow`` already defined
    ``raylib.lib`` must come before ``user32.lib`` on the link line; both
    export the symbol.  Do not reorder the project's library list.

Window does not open, or the game exits with code 2
    No usable OpenGL driver (remote desktop sessions, some VMs).  Run on the
    machine's console session or update the GPU driver.  Exit code 2 is also
    what a missing data directory reports; check the message printed first.

"Data not found" at startup
    The working directory or ``WOLF3D_DATA`` does not point at a folder with
    ``*.WL1``.  See "Running from Visual Studio".  Exact file names matter:
    the shareware set is ``AUDIOHED.WL1``, ``GAMEMAPS.WL1``, ``VGAGRAPH.WL1``
    and friends.

No sound
    Windows has no default playback device, or ``-nosound`` /
    ``WOLF3D_NOSOUND=1`` is set.  The game still runs without audio.

SmartScreen or antivirus flags the exe
    Locally built, unsigned binaries can trigger a warning.  Choose "More
    info" > "Run anyway", or exclude your build folder.

Warnings C4273 (``itoa``, ``ltoa``, ...) in ``wl_compat.h``
    Known and harmless with the CRT's own declarations.
