#!/usr/bin/env python3
"""Exercise real startup/shutdown with isolated config and silent audio."""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

if len(sys.argv) > 1 and sys.argv[1] == "--session":
    game, data, log, *flags = sys.argv[2:]
    driver = Path(__file__).resolve().parents[1] / "scripts" / "xdrive.py"
    with open(log, "w") as output:
        process = subprocess.Popen([game, "-data", data, "tedlevel", "0", "nowait"] + flags,
                                   stdout=output, stderr=subprocess.STDOUT)
        try:
            subprocess.run([sys.executable, str(driver), "wait:15", "sleep:15000",
                            "key:F10", "sleep:800", "key:y", "sleep:1500"],
                           check=True, timeout=40)
            sys.exit(process.wait(timeout=10))
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)

game, data = sys.argv[1:3]
if not Path(data, "VSWAP.WL1").exists() or not shutil.which("xvfb-run"):
    print("SKIPPED: needs shareware data and xvfb-run")
    sys.exit(0)

with tempfile.TemporaryDirectory(prefix="wolf-silent-config-") as work:
    env = dict(os.environ, XDG_CONFIG_HOME=work, WOLF3D_NOSOUND="1")
    config = Path(work, "raylib_wolf3d", "CONFIG.WL1")

    def run(flag=False):
        session_env = dict(env)
        if flag:
            session_env.pop("WOLF3D_NOSOUND")
        command = ["xvfb-run", "-a", sys.executable, "-I", str(Path(__file__).resolve()),
                   "--session", game, data, str(Path(work, "session.log"))]
        if flag:
            command.append("-nosound")
        result = subprocess.run(command, env=session_env, capture_output=True,
                                text=True, timeout=90)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr +
                               Path(work, "session.log").read_text())
        return struct.unpack_from("<3H", config.read_bytes(), 472)

    actual = run()
    assert actual == (2, 1, 3), ("fresh defaults", actual)
    print("PASS: fresh silent config retains AdLib / AdLib / Sound Blaster")
    for modes in [(2, 1, 3), (1, 0, 1), (0, 0, 0)]:
        for flag in [False, True]:
            contents = bytearray(config.read_bytes())
            struct.pack_into("<3H", contents, 472, *modes)
            config.write_bytes(contents)
            actual = run(flag)
            assert actual == modes, (modes, flag, actual)
            print("PASS: %s preserves %s" % ("-nosound" if flag else "environment", modes))
