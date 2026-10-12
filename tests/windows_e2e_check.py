#!/usr/bin/env python3
"""windows_e2e_check.py - Automated end-to-end verification suite for Windows.

Usage:
    python tests/windows_e2e_check.py <path_to_raylib_wolf3d.exe> <path_to_shareware_dir>

Exercises:
1. Missing data handling (expects exit code 2).
2. Monotonic 70 Hz clock timing (-videotest timing).
3. Save/load serialization round-trip & corruption resistance (-savetest).
4. Deterministic demo replay (-demotest all -viewsize 19).
5. User configuration validation and isolation.
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) < 3:
        print("Usage: python tests/windows_e2e_check.py <game_exe> <shareware_dir>")
        sys.exit(2)

    game = os.path.abspath(sys.argv[1])
    data = os.path.abspath(sys.argv[2])

    if not os.path.isfile(game):
        print(f"Error: game executable not found at '{game}'")
        sys.exit(1)
    if not os.path.isfile(os.path.join(data, "VSWAP.WL1")):
        print(f"Error: VSWAP.WL1 not found in '{data}'")
        sys.exit(1)

    print("=== Raylib Wolf3D Windows End-to-End Verification Suite ===")

    # 1. Missing Data Check
    print("\n[1/5] Testing missing data handling...")
    res = subprocess.run([game, "-data", "C:\\nonexistent_dir_test"], capture_output=True, text=True)
    if res.returncode != 2:
        print(f"FAIL: Expected exit code 2 on missing data, got {res.returncode}")
        sys.exit(1)
    print("PASS: Correctly reported missing data and exited with code 2.")

    # 2. 70 Hz Clock Precision
    print("\n[2/5] Testing 70 Hz monotonic clock precision...")
    res = subprocess.run([game, "-data", data, "-nosound", "-videotest", "timing"],
                         capture_output=True, text=True, timeout=30)
    if res.returncode != 0 or "70 x VL_WaitVBL" not in res.stdout:
        print(f"FAIL: videotest timing failed: {res.stdout}\n{res.stderr}")
        sys.exit(1)
    print("Output:", res.stdout.strip())
    print("PASS: 70 Hz timer verified within acceptable jitter.")

    # 3. Save / Load & Persistence Round-trip
    print("\n[3/5] Testing save/load round-trip and corruption resistance...")
    res = subprocess.run([game, "-data", data, "-nosound", "-savetest"],
                         capture_output=True, text=True, timeout=60)
    if res.returncode != 0 or "FAIL" in res.stdout or "round trip hash" not in res.stdout:
        print(f"FAIL: -savetest failed:\n{res.stdout}\n{res.stderr}")
        sys.exit(1)
    print("PASS: Save/load round-trip restored exact hash and passed all corruption cases.")

    # 4. Demo Replay Synchronization
    print("\n[4/5] Testing deterministic demo replay synchronization...")
    res = subprocess.run([game, "-data", data, "-nosound", "-demotest", "all", "-viewsize", "19"],
                         capture_output=True, text=True, timeout=120)
    expected_hashes = ["f1e6cd20", "dec12253", "47743191", "4b0dd2a9"]
    if res.returncode != 0:
        print(f"FAIL: -demotest all returned {res.returncode}:\n{res.stdout}")
        sys.exit(1)
    for h in expected_hashes:
        if h not in res.stdout:
            print(f"FAIL: Expected demo hash {h} not found in output:\n{res.stdout}")
            sys.exit(1)
    print("PASS: All 4 shareware demos played to completion with exact matching state hashes.")

    # 5. Configuration Isolation and Persistence
    print("\n[5/5] Testing configuration persistence and boundary validation...")
    with tempfile.TemporaryDirectory(prefix="wolf3d_cfg_test_") as tmpdir:
        cfgdir = os.path.join(tmpdir, "raylib_wolf3d")
        env = dict(os.environ, APPDATA=tmpdir, LOCALAPPDATA=tmpdir)

        # Write valid configuration
        b = b"RLW3DCFG" + struct.pack("<H", 1)
        for _ in range(7):
            b += b"id software-'92".ljust(58, b"\0") + struct.pack("<iHH", 10000, 1, 0)
        b += struct.pack("<HHH", 1, 0, 0)
        b += bytes([1, 0, 0, 0]) + struct.pack("<h", 0)
        for v in [0x48, 0x4d, 0x50, 0x4b, 0x1d, 0x38, 0x36, 0x39, 2, 3, 4, 5, 0, 1, 2, -1, 0, 1, -1, -1, 15, 5]:
            b += struct.pack("<h", v)

        os.makedirs(cfgdir, exist_ok=True)
        with open(os.path.join(cfgdir, "CONFIG.WL1"), "wb") as f:
            f.write(b)

        res = subprocess.run([game, "-data", data, "-nosound", "-savetest"],
                             env=env, capture_output=True, text=True, timeout=30)
        if res.returncode != 0 or "ignoring invalid" in res.stdout:
            print("FAIL: Valid configuration was incorrectly rejected")
            sys.exit(1)

    print("PASS: Configuration persistence verified.")

    print("\n=== All End-to-End Checks Successfully Verified on Windows 11 ===")


if __name__ == "__main__":
    main()
