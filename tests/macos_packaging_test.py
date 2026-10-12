#!/usr/bin/env python3
"""macOS regression fixtures for signing and portable package dependencies."""
import pathlib
import subprocess
import tempfile
import zipfile

repo = pathlib.Path(__file__).resolve().parents[1]


def run(*args, ok=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if ok and result.returncode:
        raise RuntimeError(result.stdout)
    return result


with tempfile.TemporaryDirectory(prefix='wolf-macos-packaging-') as tmp:
    src = pathlib.Path(tmp)
    build = src / 'build'
    (src / 'main.c').write_text('int main(void) { return 0; }\n')
    (src / 'library.c').write_text('void external(void) {}\n')
    # Exercise the actual production bundle block, without unrelated game code.
    cmake = (repo / 'CMakeLists.txt').read_text()
    block = cmake[cmake.index('if(APPLE)\n  option(WOLF3D_MACOS_BUNDLE'):cmake.index('\ninclude(CTest)')]
    block = block.replace('cmake/macos/Info.plist.in', str(repo / 'cmake/macos/Info.plist.in'))
    (src / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "Minimum macOS version")
project(packaging_fixture C)
add_executable(raylib_wolf3d main.c)
option(EXTERNAL "Link a non-system dylib" OFF)
if(EXTERNAL)
  add_library(review SHARED library.c)
  target_link_libraries(raylib_wolf3d PRIVATE review)
endif()
''' + block)
    run('cmake', '-S', str(src), '-B', str(build), '-G', 'Ninja')
    run('cmake', '--build', str(build))
    app = build / 'raylib_wolf3d.app'
    run('codesign', '--verify', '--strict', str(app))
    binary = app / 'Contents/MacOS/raylib_wolf3d'
    run('cmake', '-S', str(src), '-B', str(build), '-DWOLF3D_BUNDLE_VERSION=1.2.3')
    rebuilt = run('cmake', '--build', str(build))
    assert 'Linking C executable' not in rebuilt.stdout, rebuilt.stdout
    run('codesign', '--verify', '--strict', str(app))
    assert run('/usr/libexec/PlistBuddy', '-c', 'Print :CFBundleVersion', str(app / 'Contents/Info.plist')).stdout.strip() == '1.2.3'
    print('PASS: metadata-only rebuild signs updated Info.plist without relinking')
    license_dir = build / '_deps/raylib-src'
    license_dir.mkdir(parents=True)
    (license_dir / 'LICENSE').write_text('Fixture license\n')
    out = src / 'dist'
    script = str(repo / 'scripts/package_macos.sh')
    run('/bin/bash', script, '-B', str(build), '-o', str(out), '0.0.0-test')
    archive = next(out.glob('*.zip'))
    with zipfile.ZipFile(archive) as zipped:
        assert not any(name.lower().endswith(('.wl1', '.wl6', '.sod')) for name in zipped.namelist())
    extracted = src / 'extracted'
    run('ditto', '-x', '-k', str(archive), str(extracted))
    run('codesign', '--verify', '--strict', str(next(extracted.glob('*/raylib_wolf3d.app'))))
    print('PASS: system-only app packages and extracted signature verifies')
    run('cmake', '-S', str(src), '-B', str(build), '-DEXTERNAL=ON')
    run('cmake', '--build', str(build))
    rejected_out = src / 'rejected'
    result = run('/bin/bash', script, '-B', str(build), '-o', str(rejected_out), '0.0.0-test', ok=False)
    assert result.returncode != 0 and 'external runtime dependency:' in result.stdout, result.stdout
    assert not list(rejected_out.glob('*.zip'))
    print('PASS: @rpath dylib rejected before archive creation')
