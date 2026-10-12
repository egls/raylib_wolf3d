"""Run the headless platform harness with spaces, relative paths and precedence.
Windows: python tests/platform_check.py build-platform-test.exe
The harness links wl_timer.c, wl_platform.c and wl_persist.c (no raylib).
"""
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile

exe = str(Path(sys.argv[1]).resolve())
assets = ('AUDIOHED', 'AUDIOT', 'GAMEMAPS', 'MAPHEAD', 'VGADICT', 'VGAGRAPH', 'VGAHEAD', 'VSWAP')
with tempfile.TemporaryDirectory(prefix='wolf platform ') as tmp:
    root = Path(tmp)
    data = root / '-data'
    data.mkdir()
    for name in assets:
        (data / (name + '.WL1')).touch()
    for mode in ('argument', 'environment', 'current', 'fallback'):
        config = root / (mode + ' config')
        config.mkdir()
        env = os.environ.copy()
        env.update(HOME=str(config), APPDATA=str(config), LOCALAPPDATA=str(config), XDG_CONFIG_HOME=str(config))
        if mode == 'fallback':
            env.pop('APPDATA', None)
        env['WOLF3D_DATA'] = str(data)
        args = []
        cwd = root
        if mode == 'argument':
            env['WOLF3D_DATA'] = str(root / 'missing')
            args = ['-data', '-data']  # relative directory named like the flag
        elif mode == 'current':
            env.pop('WOLF3D_DATA')
            cwd = data
        subprocess.run([exe, *args], cwd=cwd, env=env, check=True)
        save = config / 'Library' / 'Application Support' if sys.platform == 'darwin' else config
        assert (save / 'raylib_wolf3d' / 'CONFIG.WL1').read_bytes() == b'test'
        assert not (data / 'CONFIG.WL1').exists()
    for args in (['-data'], ['-data', str(root / 'missing')]):
        result = subprocess.run([exe, *args], cwd=root, env=env, capture_output=True)
        assert result.returncode == 2
    if sys.platform == 'darwin':
        # Simulate Finder's unrelated cwd, both standalone and bundle launches.
        env.pop('WOLF3D_DATA', None)
        env['WOLF_TEST_MIXED_CASE'] = '1'
        for location in ('standalone', 'Wolf.app/Contents/MacOS', 'user'):
            home = root / ('home ' + location.replace('/', '_'))
            home.mkdir()
            env['HOME'] = str(home)
            binary_dir = root / location
            binary_dir.mkdir(parents=True, exist_ok=True)
            binary = binary_dir / 'wolf_platform_test'
            shutil.copy2(exe, binary)
            if location == 'user':
                destination = home / 'Library' / 'Application Support' / 'raylib_wolf3d'
            elif '.app/' in location:
                destination = binary_dir.parent / 'Resources'
            else:
                destination = binary_dir
            destination.mkdir(parents=True, exist_ok=True)
            for name in assets:
                (destination / (name.lower() + '.wL1')).touch()
            subprocess.run([str(binary)], cwd=root, env=env, check=True)
            assert (home / 'Library' / 'Application Support' / 'raylib_wolf3d' / 'CONFIG.WL1').read_bytes() == b'test'
            # Explicit selection must fail instead of falling back to bundled data.
            result = subprocess.run([str(binary), '-data', str(root / 'missing')], cwd=root, env=env)
            assert result.returncode == 2
    (data / 'VSWAP.WL1').unlink()
    result = subprocess.run([exe, '-data', str(data)], cwd=root, env=env, capture_output=True)
    assert result.returncode == 2 and b'VSWAP.WL1' in result.stderr
print('PASS: data precedence, spaces, relative paths, fallback, startup errors')
