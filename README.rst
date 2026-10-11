.. image:: docs/images/title-menu.png
   :alt: Wolfenstein 3D raylib port – title menu

Wolfenstein 3D
==============

raylib port (work in progress)
------------------------------

This fork ports the DOS source in ``WOLFSRC`` to 64-bit C on raylib 5.5.
The game logic and renderer stay as they are.  The DOS platform layer is
replaced by ``port/``, and only ``port/id_raylib.c`` includes ``raylib.h``.
``PORTING.md`` lists every non-portable piece and the stage that replaces it.
The shareware (WL1) game is playable: signon, title, menus, E1M1 and the
other shareware levels, doors, combat, death and restart, save/load and quit.
Sound plays through raylib (AdLib music and effects, PC speaker and digitized
sounds); ``-nosound`` / ``WOLF3D_NOSOUND=1`` mute it.  Anything still
unported stops with ``not ported yet: <name>`` (exit code 3).

Only the shareware episode (Wolfenstein 3D v1.4, ``*.WL1``) is supported.
The registered (``*.WL6``) and Spear of Destiny (``*.SOD``) data are not.

Build (Ubuntu 24.04; needs CMake >= 3.16, a C compiler, Python 3 and the X11/GL
development packages raylib needs)::

    sudo apt-get install cmake ninja-build python3 curl unzip \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
        libgl1-mesa-dev libasound2-dev
    cmake -B build
    cmake --build build -j2

raylib 5.5 is downloaded by CMake (pinned by SHA-256) unless an installed
raylib 5.5 is found.  ``-DWOLF3D_ALL_WARNINGS=ON`` shows the full ``-Wall``
output for the original sources.

Game data: only the freely distributable shareware v1.4 files (``*.WL1``) are
supported.  They are never committed and no release package contains them.
``scripts/fetch_shareware.sh`` downloads the shareware archive, checks its
SHA-256 and the SHA-256 of each of the eight ``*.WL1`` files, and copies them
into a cache outside the repository (``curl``, ``unzip`` and ``sha256sum``
are needed)::

    scripts/fetch_shareware.sh              # -> ~/wolf3d-data/shareware
    scripts/fetch_shareware.sh --check-only # verify an existing copy
    ./build/raylib_wolf3d -data ~/wolf3d-data/shareware
    # or: WOLF3D_DATA=~/wolf3d-data/shareware ./build/raylib_wolf3d

The expected checksums are listed at the top of the script.  If you already
own the shareware files, copy them into a directory and run
``scripts/fetch_shareware.sh --check-only DIR`` to verify them.

``WOLF3D_NOTPORTED_HOLD_MS=<ms>`` keeps the window up for that long before
the not-ported exit (for screenshots).

Config (``CONFIG.WL1``: high scores, controls, view size) and save games
(``SAVEGAM0.WL1`` ... ``SAVEGAM9.WL1``) are kept in
``$XDG_CONFIG_HOME/raylib_wolf3d/`` (``~/.config/raylib_wolf3d/`` by default).
They use this port's own little-endian format; DOS files are not read.

Debug build with ASan/UBSan, and the demo regression check::

    cmake --preset asan && cmake --build --preset asan
    ctest --preset asan
    # the demo check alone:
    xvfb-run -a ./build/raylib_wolf3d -data ~/wolf3d-data/shareware -demotest all -viewsize 19

``-demotest N|all`` plays the built-in demos at full speed and prints the
final state of each (frames played, a state hash).  The ``demo_sync`` CTest
compares them with recorded values.  The demos stay in sync only at view size
19 (see ``tests/demo_sync.cmake``).

GUI tests under Xvfb: ``scripts/xvfb_session.sh`` runs the game on a private
X server and drives it with ``scripts/xdrive.py`` (real XTest key events,
screenshots of the game window only), e.g.::

    scripts/xvfb_session.sh "" sleep:3000 key:space sleep:2000 shot:signon.png

Controls
--------

These are the original defaults.  Movement and button keys can be changed
in the Control menu (F6).

=========================  =======================================================
Key                        Action
=========================  =======================================================
Arrow keys                 Move forward/back, turn left/right
Ctrl                       Fire
Alt + Left/Right           Strafe
Right Shift                Run
Space                      Open doors, push walls, use switches
1 2 3 4                    Knife, pistol, machine gun, chain gun
Esc                        Menu
F1                         Help
F2 / F3                    Save / load game
F4 / F5 / F6               Sound, view size, control menus
F7 / F8 / F9 / F10         End game, quick save, quick load, quit
Alt + Enter                Toggle fullscreen (this port; never reaches the game)
=========================  =======================================================

The mouse works as in the original (move/turn and buttons).  The original
Pause key is not mapped by this port yet.

Command-line options
--------------------

Port options:

``-data DIR``
    Directory with the ``*.WL1`` files.  Without it the ``WOLF3D_DATA``
    environment variable is used, then the current directory.  Missing data
    exits with code 2.

``-videotest signon|title|fade|keys|timing``
    Developer harness: exercise one part of the video/input backend and
    exit (see ``port/wl_videotest.c``).

``-demotest N|all`` [``-viewsize N``]
    Play built-in demo N (0-3) or all four at full speed, print the final
    state of each and exit (1 if a demo desynced).  ``-viewsize N`` (4-19)
    overrides the configured view size; the recorded hashes need 19.  See
    ``port/wl_demotest.c``.

``-savetest``
    Check the save game format (save/load round trip, damaged files) and
    exit.  Used by the ``persist_check`` test.

Original options (``US_CheckParm`` ignores case and a leading ``-`` or ``/``):

``nowait``
    Skip the "press a key" pause on the signon screen.

``tedlevel N`` [``baby`` | ``easy`` | ``normal`` | ``hard``]
    Start map N directly at the given difficulty (the level editor launch).

``nomouse``, ``nojoys``
    Do not use the mouse or joysticks.

``goobers``
    In a game, Left Shift + Alt + Backspace then turns on the Tab + key debug
    commands (Tab+G god mode, Tab+W warp, ...).  Holding Tab while the demo
    loop starts the control panel records a demo instead.

Releases
--------

Pushing a ``v*`` tag (e.g. ``v1.0.0``) runs ``.github/workflows/release.yml``
on Ubuntu 24.04 x86_64.  It builds, runs the tests, packages, checks the
packages and attaches them to a **draft** GitHub release, which a maintainer
reviews and publishes by hand:

``raylib_wolf3d-VERSION-linux-x86_64.tar.gz``
    The stripped binary, ``scripts/fetch_shareware.sh`` and ``doc/`` (this
    README, ``SOURCE.txt`` with the commit and rebuild steps, ``licenses/``).
    Unpack it, run ``scripts/fetch_shareware.sh``, then
    ``./raylib_wolf3d -data ~/wolf3d-data/shareware``.

``raylib-wolf3d_VERSION_amd64.deb``
    Installs ``/usr/games/raylib_wolf3d`` (a launcher that defaults
    ``WOLF3D_DATA`` to ``~/wolf3d-data/shareware``) and
    ``/usr/share/raylib-wolf3d/scripts/fetch_shareware.sh``.  Install with
    ``sudo apt install ./raylib-wolf3d_VERSION_amd64.deb``.

``SHA256SUMS``
    Checksums of both packages.

Pull requests that change the packaging run the same workflow as a dry run
(the packages are uploaded as a workflow artifact, no release is made).  The
same packages can be built locally::

    cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build
    scripts/package.sh -B build -o dist --deb 0.0.0~local

Packaging needs ``dpkg-dev`` for ``--deb``.  Archive timestamps come from the
commit, so the same commit and toolchain give the same files.

Game data is not bundled.  The archive ``fetch_shareware.sh`` downloads is a
third-party repackaging (it also contains DOS and Wolf4SDL executables) and
carries none of the original shareware distribution terms (``VENDOR.DOC``,
``LICENSE``).  The 1992 shareware terms, as far as they are documented,
allow copying the shareware only as the complete, unmodified original
distribution; a release with just the ``*.WL1`` files would not be that.  So
users fetch the data themselves.

Licenses
--------

Different parts of this repository are under different licenses.  A built
binary combines them, so check all of them before redistributing one.

* ``WOLFSRC/`` (the original game source): id Software's *Limited Use
  Software License Agreement*, ``WOLFSRC/README/LICENSE.DOC`` (quoted below).
  It allows non-commercial, educational use and forbids selling or otherwise
  commercially exploiting the software.  Wolfenstein 3D is a trademark of its
  owner; this project is not affiliated with id Software or ZeniMax.
* ``port/``, ``scripts/``, ``tests/``, ``CMakeLists.txt`` and the packaging:
  new code written for this port.  It has no license file of its own and is
  distributed together with the game source.
* ``audio/`` (WOLF-3): GPL-2.0-or-later, ``audio/LICENSE``.  ``audio/vendor/``
  is Nuked OPL3 1.8, Copyright (C) 2013-2020 Nuke.YKT, LGPL-2.1-or-later,
  ``audio/vendor/LICENSE``.  See ``audio/README.md`` for provenance.  When a
  binary links them, provide the corresponding source (the release notes name
  the commit) so the library can be modified and the program rebuilt.
* raylib 5.5 (fetched at build time, statically linked): zlib/libpng
  license, Copyright (c) 2013-2024 Ramon Santamaria.  Its license text ships
  in every package under ``doc/licenses/``.
* Game data (``*.WL1``): copyright id Software, not in this repository and
  not in any package.

Release packages include ``doc/licenses/`` with the id license, the raylib
license and, when the audio engine is linked in, the GPL and LGPL texts.

Headless core verification
--------------------------

The native memory/page/cache managers, codecs, raycaster, and looping scalers
can be tested without a monitor or audio device::

    scripts/fetch_shareware.sh --check-only
    cmake -B build -DWOLF3D_CORE_SANITIZERS=ON
    cmake --build build -j2
    ctest --test-dir build --output-on-failure

``core_codecs`` and ``core_renderer`` run without data. ``core_wl1_data``
skips with code 77 when the external directory is absent; set
``-DWOLF3D_TEST_DATA=/path/to/shareware`` to override its location. It checks
E1M1 plane CRC32s, wall/sprite/title CRC32s, all ten maps, all graphics/audio
chunks, every resident sprite at several scales, and cache/purge/shutdown
cycles. SHA-256 verification stays in ``scripts/fetch_shareware.sh``.

The WOLF-6 Sound Manager drives the Nuked OPL engine and 7042 Hz VSWAP
samples through a raylib AudioStream. ``-nosound`` / ``WOLF3D_NOSOUND=1``
disable all audio; ``-nomusic`` / ``WOLF3D_NOMUSIC=1`` disable music only.
Audio integration tests and device recordings are described in
``audio/README.md``.

``demo_sync`` (see above) needs the data and ``xvfb-run``, and skips without
them.

WebAssembly build
-----------------

The game also builds for the browser with Emscripten (raylib's web platform,
WebGL).  It needs emsdk (tested with emcc 6.0.10) and, to be playable, the
shareware data, which the build bundles from a local directory into
``raylib_wolf3d.data``::

    source ~/emsdk/emsdk_env.sh
    scripts/fetch_shareware.sh                  # -> ~/wolf3d-data/shareware
    cmake --preset web                          # -DWOLF3D_WEB_DATA=<dir> for another directory
    cmake --build --preset web
    python3 -m http.server -d build-web 8000    # open http://localhost:8000/raylib_wolf3d.html

``build-web/`` then holds ``raylib_wolf3d.html``, ``.js``, ``.wasm`` and
``.data``; serve the four files from any static web server.  The build checks
the SHA-256 of each ``*.WL1`` file.  Without the data (``-DWOLF3D_WEB_DATA=``)
the page builds but only says that the data is missing.

How it works: the game keeps its blocking loops (menus, fades, "press a key",
demos) and runs under ASYNCIFY.  The frame and input pump in
``port/id_raylib.c`` and the timer waits in ``port/wl_timer.c`` give the
browser its turn with ``emscripten_sleep``.  There is no timer thread: the
70 Hz ``TimeCount`` catches up with ``emscripten_get_now()`` whenever the game
reads the clock or waits.  The page (``web/shell.html``) starts the game on a
click, keeps game keys (arrows, Space, Tab, F1-F10, Alt, Ctrl...) from
scrolling or triggering browser shortcuts, captures the mouse with pointer lock
after a click on the game (Esc lets it go), and does Alt+Enter fullscreen.  The
canvas fills the window and the 320x200 frame is letterboxed to 4:3.

Limits for now: no sound (the page runs the game with ``-nosound``), and
config and saves live in memory only, so they are lost on reload.
``?args=...`` adds command-line options, e.g. ``?args=-tedlevel%200%20-nowait``.

``scripts/web_smoke.mjs`` checks a build in headless Chromium or Firefox with
Playwright: the four demos must reach the same state hashes as the native
``demo_sync`` test, and a normal start must reach the title, the menu and
E1M1, walk, and turn with the mouse, with no page or console errors::

    PLAYWRIGHT=/path/to/node_modules/playwright/index.mjs \
        node scripts/web_smoke.mjs build-web web-smoke [--browser firefox]

Original release notes
----------------------

The original source release was made using the ``DEICE.EXE``
installer (DeIce) in the root directory.  (Basically a way to
split self-extracting zip files over multiple floppy disks.)
After execution, that installer will create the contents of
the ``WOLFSRC`` directory.  In the "installed" directory, the
the ``README.TXT``, ``RELEASE.TXT``, and ``LICENCE.DOC`` reside
in the ``README`` directory.  They are duplicated below.

Travis "Ash" Bradshaw <travis.bradshaw@idsoftware.com>

February 6, 2012

RELEASE.TXT
-----------

::

  We are releasing this code for the entertainment of the 
  user community.  We don't guarentee that anything even 
  builds in here.  Projects just seem to rot when you leave 
  them alone for long periods of time.

  This is all the source we have relating to the original 
  PC wolfenstein 3D project.  We haven't looked at this 
  stuff in years, and I would probably be horribly embarassed 
  to dig through my old code, so please don't ask any questions 
  about it. The original project was built in borland c++ 3.0.  
  I think some minor changes were required for later versions.

  You will need the data from a released version of wolf or spear 
  to use the exe built from this code.  You can just use a 
  shareware version if you are really cheap.


  Some coding comments in retrospect:

  The ray casting refresh architecture is still reasonably 
  appropriate for the game.  A BSP based texture mapper could 
  go faster, but ray casting was a lot simpler to do at the time.

  The dynamically compiled scaling routines are now a Bad Thing.  
  On uncached machines (the original target) they are the fastest 
  possible way to scale walls, but on modern processors you just 
  wind up thrashing the code cash and wrecking performance.  
  A simple looping texture mapper would be faster on 486+ machines.

  The whole page manager caching scheme is unecessarily complex.

  Way too many #ifdefs in the code!


  Some project ideas with this code:

  Add new monsters or weapons.

  Add taller walls and vertical motion.   This should only be 
  done if the texture mapper is rewritten.

  Convert to a 32 bit compiler.  This would be a fair amount 
  of work, but I would hate to even mess with crusty old 16 
  bit code.  The code would get a LOT smaller.

  Make a multi-player game that runs on DOOM sersetup / ipxsetup drivers.


  Have fun...

  John Carmack
  Technical Director
  Id Software

README.TXT
----------

NOTES:

This version will compile under BORLAND C++ 3.0/3.1 and compiled perfectly
before it was uploaded. 

Please do not send your questions to id Software.

LICENCE.DOC
-----------

 :: 
  	LIMITED USE SOFTWARE LICENSE AGREEMENT

  	This Limited Use Software License Agreement (the "Agreement") 
  is a legal agreement between you, the end-user, and Id Software, Inc. 
  ("ID").  By continuing the downloading of this Wolfenstein 3D 
  (the "Trademark") software material, which includes source code 
  (the "Source Code"), artwork data, music and software tools 
  (collectively, the "Software"), you are agreeing to be bound by the 
  terms of this Agreement.  If you do not agree to the terms of this 
  Agreement, promptly destroy the Software you may have downloaded.  

  ID SOFTWARE LICENSE

  	Grant of License.  ID grants to you the right to use one (1) 
  copy of the Software on a single computer.  You have no ownership or 
  proprietary rights in or to the Software, or the Trademark.  For purposes 
  of this section, "use" means loading the Software into RAM, as well as 
  installation on a hard disk or other storage device.  The Software, 
  together with any archive copy thereof, shall be destroyed when no longer 
  used in accordance with this Agreement, or when the right to use the 
  Software is terminated.  You agree that the Software will not be shipped, 
  transferred or exported into any country in violation of the U.S. 
  Export Administration Act (or any other law governing such matters) and 
  that you will not utilize, in any other manner, the Software in violation 
  of any applicable law.

  	Permitted Uses.  For educational purposes only, you, the end-user, 
  may use portions of the Source Code, such as particular routines, to 
  develop your own software, but may not duplicate the Source Code, except 
  as noted in paragraph 4.  The limited right referenced in the preceding 
  sentence is hereinafter referred to as "Educational Use."  By so exercising 
  the Educational Use right you shall not obtain any ownership, copyright,
  proprietary or other interest in or to the Source Code, or any portion of
  the Source Code.  You may dispose of your own software in your sole
  discretion.  With the exception of the Educational Use right, you may not
  otherwise use the Software, or an portion of the Software, which includes
  the Source Code, for commercial gain.

  	Prohibited Uses:  Under no circumstances shall you, the end-user, 
  be permitted, allowed or authorized to commercially exploit the Software.
  Neither you nor anyone at your direction shall do any of the following acts
  with regard to the Software, or any portion thereof:

  	Rent;

  	Sell;

  	Lease;

  	Offer on a pay-per-play basis;

  	Distribute for money or any other consideration; or

  	In any other manner and through any medium whatsoever commercially 
  exploit or use for any commercial purpose.

  Notwithstanding the foregoing prohibitions, you may commercially exploit the
  software you develop by exercising the Educational Use right, referenced in
  paragraph 2. hereinabove. 

  	Copyright.  The Software and all copyrights related thereto 
  (including all characters and other images generated by the Software 
  or depicted in the Software) are owned by ID and is protected by 
  United States copyright laws and international treaty provisions.  
  Id shall retain exclusive ownership and copyright in and to the Software 
  and all portions of the Software and you shall have no ownership or other 
  proprietary interest in such materials. You must treat the Software like 
  any other copyrighted material, except that you may either (a) make one 
  copy of the Software solely for back-up or archival purposes, or (b) 
  transfer the Software to a single hard disk provided you keep the original 
  solely for back-up or archival purposes.  You may not otherwise reproduce, 
  copy or disclose to others, in whole or in any part, the Software.  You 
  may not copy the written materials accompanying the Software.  You agree 
  to use your best efforts to see that any user of the Software licensed 
  hereunder complies with this Agreement.

  	NO WARRANTIES.  ID DISCLAIMS ALL WARRANTIES, BOTH EXPRESS IMPLIED,
  INCLUDING BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND
  FITNESS FOR A PARTICULAR PURPOSE WITH RESPECT TO THE SOFTWARE.  THIS LIMITED
  WARRANTY GIVES YOU SPECIFIC LEGAL RIGHTS.  YOU MAY HAVE OTHER RIGHTS WHICH
  VARY FROM JURISDICTION TO JURISDICTION.  ID DOES NOT WARRANT THAT THE
  OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED, ERROR FREE OR MEET YOUR
  SPECIFIC REQUIREMENTS.  THE WARRANTY SET FORTH ABOVE IS IN LIEU OF ALL OTHER
  EXPRESS WARRANTIES WHETHER ORAL OR WRITTEN.  THE AGENTS, EMPLOYEES,
  DISTRIBUTORS, AND DEALERS OF ID ARE NOT AUTHORIZED TO MAKE MODIFICATIONS TO
  THIS WARRANTY, OR ADDITIONAL WARRANTIES ON BEHALF OF ID. 

  	Exclusive Remedies.  The Software is being offered to you free of any
  charge.  You agree that you have no remedy against ID, its affiliates,
  contractors, suppliers, and agents for loss or damage caused by any defect
  or failure in the Software regardless of the form of action, whether in
  contract, tort, includinegligence, strict liability or otherwise, with
  regard to the Software.  This Agreement shall be construed in accordance
  with and governed by the laws of the State of Texas.  Copyright and other
  proprietary matters will be governed by United States laws and international
  treaties.  IN ANY CASE, ID SHALL NOT BE LIABLE FOR LOSS OF DATA, LOSS OF
  PROFITS, LOST SAVINGS, SPECIAL, INCIDENTAL, CONSEQUENTIAL, INDIRECT OR OTHER
  SIMILAR DAMAGES ARISING FROM BREACH OF WARRANTY, BREACH OF CONTRACT,
  NEGLIGENCE, OR OTHER LEGAL THEORY EVEN IF ID OR ITS AGENT HAS BEEN ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY.
  Some jurisdictions do not allow the exclusion or limitation of incidental or
  consequential damages, so the above limitation or exclusion may not apply to
  you.

  	General Provisions.  Neither this Agreement nor any part or portion 
  hereof shall be assigned, sublicensed or otherwise transferred by you.  
  Should any provision of this Agreement be held to be void, invalid, 
  unenforceable or illegal by a court, the validity and enforceability of the 
  other provisions shall not be affected thereby.  If any provision is 
  determined to be unenforceable, you agree to a modification of such 
  provision to provide for enforcement of the provision's intent, to the 
  extent permitted by applicable law.  Failure of a party to enforce any 
  provision of this Agreement shall not constitute or be construed as a 
  waiver of such provision or of the right to enforce such provision.  If 
  you fail to comply with any terms of this Agreement, YOUR LICENSE IS 
  AUTOMATICALLY TERMINATED and you agree to the issuance of an injunction 
  against you in favor of Id.  You agree that Id shall not have to post 
  bond or other security to obtain an injunction against you to prohibit 
  you from violating Id's rights.

  	YOU ACKNOWLEDGE THAT YOU HAVE READ THIS AGREEMENT, THAT YOU 
  UNDERSTAND THIS AGREEMENT, AND UNDERSTAND THAT BY CONTINUING THE 
  DOWNLOADING OF THE SOFTWARE, YOU AGREE TO BE BOUND BY THIS AGREEMENT'S 
  TERMS AND CONDITIONS. YOU FURTHER AGREE THAT, EXCEPT FOR WRITTEN SEPARATE 
  AGREEMENTS BETWEEN ID AND YOU, THIS AGREEMENT IS A COMPLETE AND EXCLUSIVE 
  STATEMENT OF THE RIGHTS AND LIABILITIES OF THE PARTIES.  THIS AGREEMENT 
  SUPERSEDES ALL PRIOR ORAL AGREEMENTS, PROPOSALS OR UNDERSTANDINGS, AND 
  ANY OTHER COMMUNICATIONS BETWEEN ID AND YOU RELATING TO THE SUBJECT MATTER 
  OF THIS AGREEMENT 
  

Registered data variant (WL6)
-----------------------------

Both native and web configurations also build ``wolf3d-wl6``. It uses the
original registered headers and accepts the 149-graphics-chunk WL6 layout;
registered files must be supplied by their owner at runtime and are never
bundled. The existing ``raylib_wolf3d`` target remains the shareware build.
See ``docs/data-variants.md`` for the web shell's ``variants.json`` contract,
synthetic checks, supported layout and the owner's gameplay checklist.
