#!/usr/bin/env python3
"""persist_check.py - CTest driver for the config and save game formats (WOLF-7).

1. "raylib_wolf3d -savetest": save/load round trip and damaged save files
   (see port/wl_demotest.c).
2. CONFIG.WL1 files written by this script (format in WL_MAIN.C
   ReadConfigFile): a valid one must be accepted, ones with out-of-range scan
   codes or button mappings must be ignored with a message.

    persist_check.py GAME DATA WORKDIR

Prints SKIPPED (CTest SKIP_REGULAR_EXPRESSION) without data or xvfb-run.
"""
import os
import shutil
import struct
import subprocess
import sys

game, data, work = sys.argv[1:4]
if not os.path.exists(os.path.join(data, "VSWAP.WL1")) or not shutil.which("xvfb-run"):
    print("SKIPPED: needs shareware data and xvfb-run")
    sys.exit(0)

cfgdir = os.path.join(work, "raylib_wolf3d")
env = dict(os.environ, XDG_CONFIG_HOME=work, ALSA_CONFIG_PATH="")


def run(*args):
    p = subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 640x480x24", game, "-data", data] + list(args),
                       env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                       errors="replace", timeout=240)
    return p.returncode, p.stdout


def config(dirscan=(0x48, 0x4d, 0x50, 0x4b), buttonscan=(0x1d, 0x38, 0x36, 0x39, 2, 3, 4, 5),
           buttonmouse=(0, 1, 2, -1), buttonjoy=(0, 1, -1, -1), viewsize=15):
    b = b"RLW3DCFG" + struct.pack("<H", 1)
    for i in range(7):
        b += b"id software-'92".ljust(58, b"\0") + struct.pack("<iHH", 10000, 1, 0)
    b += struct.pack("<HHH", 1, 0, 0)            # sound, music, digi modes
    b += bytes([1, 0, 0, 0]) + struct.pack("<h", 0)
    for v in list(dirscan) + list(buttonscan) + list(buttonmouse) + list(buttonjoy) + [viewsize, 5]:
        b += struct.pack("<h", v)
    os.makedirs(cfgdir, exist_ok=True)
    with open(os.path.join(cfgdir, "CONFIG.WL1"), "wb") as f:
        f.write(b)


failed = False
shutil.rmtree(work, ignore_errors=True)

rc, out = run("-savetest")
print(out)
if rc != 0 or "FAIL" in out or out.count("savetest: ok") != 9 or "round trip" not in out:
    print("persist_check: -savetest failed (exit %d)" % rc)
    failed = True

cases = [
    ("valid config", {}, True),
    ("button scan code -1", {"buttonscan": (-1, 0x38, 0x36, 0x39, 2, 3, 4, 5)}, False),
    ("direction scan code 32767", {"dirscan": (0x48, 0x4d, 32767, 0x4b)}, False),
    ("mouse button mapping 8", {"buttonmouse": (0, 1, 2, 8)}, False),
    ("joystick button mapping -2", {"buttonjoy": (0, -2, -1, -1)}, False),
]
for what, kw, accept in cases:
    config(**kw)
    rc, out = run("-demotest", "3", "-viewsize", "19")
    ignored = "ignoring invalid" in out
    ok = rc == 0 and ignored != accept
    print("persist_check: %s %s: %s" % ("ok  " if ok else "FAIL", what, "ignored" if ignored else "accepted"))
    failed |= not ok

shutil.rmtree(work, ignore_errors=True)
sys.exit(1 if failed else 0)
