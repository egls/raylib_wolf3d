# Browser audio (WOLF-42)

The existing Nuked OPL/IMF/PC/7042 Hz PCM mixer feeds the same raylib
AudioStream as native builds. Both producer and miniaudio ScriptProcessor
callback run on the browser main thread. No pthread, AudioWorklet, or
SharedArrayBuffer is required. IMF timing remains 700 Hz (560 Hz in the
standalone Spear configuration); no synthesis algorithms change here.

The page starts from a click. Browser autoplay policy gates the AudioContext;
backend click, touchend, and keydown listeners resume it. A suspended context
stops synthesis, not device detection, so it does not force the sound menu Off.
On visibility loss, suspend the context and silence callbacks immediately.
On resume or a producer gap over 250 ms, discard the ring and rebase the
wall-clock cap. The sequencer continues from its paused sample cursor, without
rendering missed background time. Game simulation itself is not paused.

Web VBL sleeps pump audio in at most 8 ms intervals. The ring contains at most
1536 frames (32 ms), with a fixed 512-frame prefetch allowance in the 1000 ppm
wall-clock cap. Native keeps its 3840-frame/80 ms queue. A checked generated
copy of raylib 5.5's raudio.c sets the web device period to 256 frames, because
raylib only exposes stream buffer size and miniaudio otherwise uses 2048.
Fetched source and native raylib configuration remain untouched. HEAPF32 is
exported for miniaudio and refreshed by Emscripten on memory growth.

## Reproduce capture and tests

Keep only verified shareware WL1 data outside the repository, as described in
the root README. No game data or recordings belong in git.

```sh
source "$HOME/emsdk/emsdk_env.sh"
cmake --preset web -DBUILD_TESTING=OFF -DWOLF3D_WEB_AUDIO_TRACE=ON
cmake --build --preset web -j 6
PLAYWRIGHT=/path/to/playwright/index.mjs \
  node scripts/web_audio.mjs build-web ../audio-evidence
# CHROMIUM=/path/to/chrome-headless-shell can override Playwright's binary.
cmake --preset release
cmake --build --preset release -j 6
ctest --test-dir build -R 'audio_ring|sound_manager|audio_contract|web_audio_policy' --output-on-failure
```

The opt-in trace timestamps **accepted synthesis events**, avoiding menu input
processing delay in latency measurements. Production builds default to tracing
Off. The harness taps miniaudio's real ScriptProcessor output at wall-clock
speed in a 640x480 viewport and writes stereo WAVs and result.json. It exercises title music, FX
none/PC/AdLib, digitized off/on, music off/on, and in-game PCM pistol shots.
The digitized-on menu preview uses an AdLib effect; the game capture confirms
accepted PCM events separately. Silence allows the OPL release tail below
0.001 full scale rather than requiring mathematically zero samples.

Latency uses the first sample above 0.001 full scale and the AudioContext
playback timestamp. It measures delivery to WebAudio, not acoustic speaker
latency. result.json reports the device's base/output latency separately.
Capture-phase underrun counts are snapshotted before exporting large sample
arrays; cumulative counters can also include stalls during screenshots and
sample transfer. Underruns output silence and never synthesize in the callback.
The hidden-tab test dispatches a deterministic visibility event in headless
Chromium, and the long-stall test blocks the main thread for 400 ms. These do
not replace verification of actual background tabs on desktop/mobile browsers.

`Module.wolfAudioStats` exposes queuedFrames, generatedFrames, underruns,
resyncs, and running for browser verification. The native web_audio_policy
harness scripts suspension, callback-before-pump ordering, a minute hidden,
irregular frames, the racing-sink cap, and a long gap without a browser.

## Remaining integration dependency

WOLF-28's silent-run preference fix is PR #12 and is not part of the master
base used here. Browser suspension preserves device availability and therefore
avoids that fallback. Explicit `-nosound` and device-failure persistence still
require PR #12 to merge, followed by a browser config round-trip check; this
change does not claim that unmerged fix is present. WOLF-45 must also verify
actual hidden tabs and other browsers before deployment.
