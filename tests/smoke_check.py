#!/usr/bin/env python3
"""Run no-data smoke test (exit codes 2 and 1, as in Linux CI).
Usage: python tests/smoke_check.py <path_to_raylib_wolf3d>
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <path-to-raylib_wolf3d>", file=sys.stderr)
    sys.exit(2)

game = str(Path(sys.argv[1]).resolve())

# Test 1: Missing data directory must exit with code 2
with tempfile.TemporaryDirectory(prefix="wolf_smoke_nodata_") as tmp:
    nodata_dir = Path(tmp) / "nonexistent"
    result = subprocess.run([game, "-data", str(nodata_dir)], capture_output=True, text=True)
    assert result.returncode == 2, (
        f"Missing data directory: expected exit code 2, got {result.returncode}.\n"
        f"Output:\n{result.stdout}\n{result.stderr}"
    )

# Test 2: Empty placeholder files must be rejected by VSWAP loader with exit code 1
with tempfile.TemporaryDirectory(prefix="wolf_smoke_fake_") as tmp:
    fake_dir = Path(tmp)
    for name in ("AUDIOHED", "AUDIOT", "GAMEMAPS", "MAPHEAD", "VGADICT", "VGAGRAPH", "VGAHEAD", "VSWAP"):
        (fake_dir / f"{name}.WL1").touch()

    cmd = [game, "-data", str(fake_dir)]
    if sys.platform != "win32" and not os.environ.get("DISPLAY") and shutil.which("xvfb-run"):
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1024x768x24"] + cmd

    result = subprocess.run(cmd, capture_output=True, text=True)
    combined = (result.stdout or "") + (result.stderr or "")
    # GL-less Windows runners (e.g. github windows-latest) have no OpenGL driver.
    # The game must say so and exit 2 (it used to die in raylib's InitWindow with
    # an access violation).  That is the only accepted skip; the headless
    # -datacheck run below still rejects the placeholders everywhere.
    if (
        sys.platform == "win32"
        and result.returncode == 2
        and "no OpenGL 3.3 driver available" in combined
    ):
        print("NOTE: no OpenGL driver; game exited cleanly (2) instead of opening a window")
        gl_less = True
    else:
        gl_less = False
    if not gl_less:
        assert result.returncode == 1, (
            f"Empty placeholder files: expected exit code 1, got {result.returncode}.\n"
            f"Output:\n{combined}"
        )
        assert "VSWAP: short read" in combined, (
            f"Expected 'VSWAP: short read' in stderr/stdout, got:\n{combined}"
        )

    # Headless structural check: needs no window or GL, so it runs on every runner.
    result = subprocess.run([game, "-data", str(fake_dir), "-datacheck"], capture_output=True, text=True)
    combined = (result.stdout or "") + (result.stderr or "")
    assert result.returncode == 1, (
        f"-datacheck on placeholder files: expected exit code 1, got {result.returncode}.\n"
        f"Output:\n{combined}"
    )
    assert "invalid, mismatched or truncated" in combined, (
        f"Expected the CA data rejection message, got:\n{combined}"
    )

print("PASS: smoke tests (missing data exit 2, placeholder data exit 1; windowed run %s)"
      % ("skipped: no OpenGL" if gl_less else "'VSWAP: short read'"))
