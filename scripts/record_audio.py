#!/usr/bin/env python3
"""Capture the ID_SD device harness from a private PipeWire/Pulse null sink.
No speakers, no game data copies, no process left running after completion.
"""
import argparse
import os
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('harness', type=Path)
p.add_argument('data', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
sink = 'wolf6_capture_' + str(os.getpid())
module = subprocess.check_output(['pactl', 'load-module', 'module-null-sink',
    'sink_name=' + sink, 'rate=48000', 'channels=2'], text=True).strip()
recorder = None
try:
    recorder = subprocess.Popen(['ffmpeg', '-nostats', '-loglevel', 'warning',
        '-y', '-f', 'pulse', '-i', sink + '.monitor', '-ar', '48000',
        '-ac', '2', '-c:a', 'pcm_s16le', str(a.output.resolve())], stdin=subprocess.PIPE)
    time.sleep(.5)
    env = dict(os.environ, PULSE_SINK=sink)
    start = time.monotonic()
    subprocess.run([str(a.harness.resolve()), str(a.data.resolve())],
                   env=env, check=True, timeout=30)
    print('Harness wall seconds:', round(time.monotonic()-start, 3))
    time.sleep(.3)
finally:
    try:
        if recorder:
            try:
                recorder.communicate(b'q', timeout=10)
            except subprocess.TimeoutExpired:
                recorder.kill()  # exact child started above
                recorder.communicate(timeout=10)
    finally:
        subprocess.run(['pactl', 'unload-module', module], check=True)
if recorder.returncode:
    raise SystemExit(recorder.returncode)
