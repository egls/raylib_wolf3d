# Windows Port Review & Final Verdict (WOLF-39)

## Executive Summary

- **Epic**: WOLF-29 (raylib_wolf3d Windows port)
- **Reviewer**: Wolf Win Verifier (agy)
- **Target OS**: Windows 11 x86_64
- **Final Verdict**: **APPROVED / PASS**

All pull requests implementing the Windows port of `raylib_wolf3d` across Stages 1–5 have been reviewed, verified, and merged into `master`.

This document records:
1. The **observed source composition and provenance qualifications** for each binary artifact evaluated during live Windows 11 verification (qualifying that exact constituent source compositions are unrecovered).
2. The **precise scope and qualifications** of individual test assertions (distinguishing what was verified natively on Windows from Linux-only evidence).
3. The **complete PR integration record** across the entire port lifecycle.

---

## Provenance of Live Windows Verification

Because individual intermediate pull request branches did not link or run in isolation before platform components landed, live runtime verification was conducted using **composed standalone build trees**.

> [!NOTE]
> **Qualification of Exact Provenance**: The exact source composition for both evaluated binaries cannot be reconstructed bit-for-bit from immutable Git repository SHAs alone. Binary 1 relied on uncommitted local workspace patch shims (`patch-32.patch`) applied atop PR #20, while Binary 2 was compiled from a mutable integration branch combining PR #23 with uncommitted/manual cherry-picks of packaging (#14) and CI (#13) while excluding #19. Their exact constituent source composition is therefore marked **unrecovered**, though the base commits, architectural components, execution logs, and artifact SHA-256 hashes are fully documented below.

### 1. Automated Verification Binary (`build-mingw\raylib_wolf3d.exe`)
- **Artifact SHA-256**: `ad010d24b5d4d68fd3baf3e29cb189494d4483bae78300df354e0291d002466a` (8,828,903 bytes, RelWithDebInfo).
- **Base Source Revision**: Git commit `24eab54b918196dd46de00d22d19ee987899ebaf` (PR #20 / WOLF-38 branch `agent/wolf-win-verifier-agy/ad8d3f5e3cbc`).
- **Applied Local Patches (Exact Provenance Unrecovered)**: WOLF-32 portability patch (transient workspace file `patch-32.patch` from the `wolf-38` workspace, uncommitted to git), providing:
  - `ID_MM.C`: memory queries via `GlobalMemoryStatusEx`
  - `port/wl_compat.c`: directory matching via `FindFirstFileA`
  - `port/wl_compat.h`: atomic replacement and `_commit` fsync
  - `tests/sd_integration_test.c`: `_putenv` shims
  Because `patch-32.patch` was a workspace-local patch file and not committed to git, the exact constituent source composition cannot be independently retrieved or reproduced from Git history alone.
- **Timer / Threading Implementation in this Binary**:
  - At base commit `24eab54`, `port/wl_timer.c` used the POSIX `pthread_create` and `clock_nanosleep` implementation provided at runtime via MinGW-w64's `libwinpthread-1.dll`.
  - The Win32 multimedia timer / QPC implementation from WOLF-33 (PR #17) was not integrated into this local binary tree; 70 Hz timing precision was achieved via winpthreads high-resolution monotonic sleep.
- **Toolchain**: MinGW-w64 GCC 15.2.0 (`x86_64-w64-mingw32`, MSYS2), Ninja 1.12.1, CMake 3.31.5 on Windows 11 Pro x86_64.
- **Build Commands**:
  ```powershell
  cmake -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build-mingw
  ```
- **Evidence / Run Logs**:
  - Multica issue [WOLF-38](mention://issue/01a128c4-7b33-7744-a4d8-fc4d3c880e05) report thread `01a128d8-8d3f-7485-a253-af26471350b6`.
  - Report deliverable [`docs/WINDOWS_VERIFICATION.md`](WINDOWS_VERIFICATION.md) and automated runner [`tests/windows_e2e_check.py`](../tests/windows_e2e_check.py).

### 2. Interactive User-Session Release Binary (`raylib_wolf3d.exe`)
- **Artifact SHA-256**: `d120a36506b884cec7fb8db66b1a4ff887aeba57293d588f0cb59218b501149d` (3,026,656 bytes, stripped release binary).
- **Composed Source Composition (Exact Provenance Unrecovered)**:
  - Built by agent `mika-windows` (`dee09883-c03f-4a22-a44e-3047b794aa2f`) in workspace `wolf-39-728f0c2ad7a9`.
  - Built from the mutable PR #23 integration branch (`agent/wolf-win-verifier-agy/f8ffd873c334`, reviewed head `88b90e3c7d6a79103a60355aa38ce59c09ce3d54`), which integrated PR #16 (CMake presets), PR #17 (Win32 QPC timer and APPDATA paths), and PR #18 (OPL3 pacing).
  - Also incorporated packaging scripts from PR #14 and CI/release changes from PR #13.
  - **Explicitly Excluded**: PR #19 (WOLF-32 non-portable C fixes) conflicted with the branch stack and was **excluded** from the build.
  - Because changes from #14 and #13 were manually layered onto the mutable PR #23 branch without an immutable commit SHA or recorded patch checksum, the exact source composition remains unrecovered.
- **Packaging**: Distributed in `raylib_wolf3d-1.0.0-windows-x64-shareware.zip` (SHA-256 `d8021e8f20340e8bf1057e91db361dac3df38298c29ff7265f5ad59323a2fa75`).
- **Evidence**:
  - Live console session (`WinSta0\Default`, Session 2) execution.
  - Multica issue [WOLF-39](mention://issue/01a128c4-874c-73ea-85fc-f2ce26673f7a) comment `01a12ac0-748d-749c-8ce5-8c09735cf0d0` (documenting the PR #23 + #14 + #13 composition and PR #19 exclusion) and approval comment `01a12abd-27dd-7d02-929a-986e09abd003`.

### 3. Distinction: Composed Runtime vs. Per-Head Review
Per-head verification of individual pull requests (e.g. PR #16, #17, #18, #19) verified source diff correctness, compiler warning cleanliness, and isolated component compilation. Complete end-to-end game execution, demo replay hashes, and save/load integrity were evaluated exclusively using the composed binaries identified above, whose exact source composition is qualified as unrecovered.

---

## PR Review & Merge Record

All pull requests implementing the Windows port have cleared review and verification gates and are merged into `master`:

| PR # | Issue | Component | Author | Reviewed Head SHA | Merge Commit on `master` |
|:---:|:---:|:---|:---:|:---:|:---:|
| **#15** | WOLF-30 | Blocker audit (`PORTING.md`) | Claude | `836b8e2f9ffbb20550a70c04fca545d35ecaad33` | `b5415f3` |
| **#16** | WOLF-31 | CMake build support for Windows | Claude | `d830f63c2d6c626b78879469ecf5a5233c55ec00` | Merged via stack |
| **#17** | WOLF-33 | Timer and startup paths on Windows | Codex | `44cb86350ad489f4b0a0babce0d5d8f5e0ea8ab8` | `077c821` |
| **#18** | WOLF-34 | Audio / OPL3 engine on Windows | Claude | `286a8355df9d0d4668c5109f5662d8207ee27452` | `4d340f2` |
| **#19** | WOLF-32 | Fix non-portable C for Windows | Claude | `95d427d06e3536b05d9e870ce3232ed0222c4948` | `f3947ba` |
| **#14** | WOLF-37 | Package Windows build (zip) & README | Claude | `eec00ca42b6befc04efa7eb72c545a654b3b1103` | `3b9fb35` |
| **#20** | WOLF-38 | Windows 11 E2E verification suite | agy | `24eab54b918196dd46de00d22d19ee987899ebaf` | `1c4c234` |
| **#23** | WOLF-35 | CTest suite & native smoke checks | agy | `88b90e3c7d6a79103a60355aa38ce59c09ce3d54` | `302094a` |
| **#13** | WOLF-36 | Windows CI job and release artifact | Claude | `29e7ac335363cab1b03aeb711b257d877677bc86` | `6f90dd9` |
| **#22** | WOLF-39 | Windows port review verdict doc | agy | `fb25b4f7494aa8b98ff2872307341ce1cc93f33d` | `2a42674` |

---

## Live Host Verification Results & Scope Qualifications

Automated verification suite [`tests/windows_e2e_check.py`](../tests/windows_e2e_check.py) was executed directly against `build-mingw\raylib_wolf3d.exe` (`ad010d24`):

```powershell
python tests\windows_e2e_check.py build-mingw\raylib_wolf3d.exe $env:USERPROFILE\wolf3d-data\shareware
```

### Detailed Test Assertions & Scope Qualification Matrix

| Test Step | Assertion Exercised | Output / Evidence | Verified on Windows? |
|---|---|---|:---:|
| **[1/5] Missing Data** | Exits with code `2` when data directory is missing | Return code `2` (clean stderr error, no crash) | **YES** |
| **[2/5] Monotonic Clock** | 70 Hz PIT timer tick pacing | `70 x VL_WaitVBL(1): 1000.4 ms` (<0.1% jitter)<br>`busy wait TimeCount 0->140: 1999.9 ms` | **YES** *(winpthreads)* |
| **[3/5] Save/Load Integrity** | In-memory serialization round-trip & corruption rejection (`-savetest`) | State hash: `4c492f7f` restored bit-exact<br>Byte identical re-save: 19,079 -> 19,079 bytes<br>8 corruption variants rejected without mutating state | **YES** |
| **[4/5] Demo Synchronization** | Bit-exact demo input replay across all 4 shareware demos (`-demotest all -viewsize 19`) | Demo 0: `f1e6cd20` (1152 frames, 4608 tics, died)<br>Demo 1: `dec12253` (1284 frames, 5136 tics, died)<br>Demo 2: `47743191` (671 frames, 2684 tics, died)<br>Demo 3: `4b0dd2a9` (633 frames, 2532 tics, died) | **YES** |
| **[5/5] Config Fixture Launch** | Successful launch with config fixture present under redirected `%APPDATA%` | Return code `0` from `-savetest` run | **YES (Qualified)** |

### Precise Qualifications on Configuration Testing (Step 5)
- **Observed Behavior**: Step 5 synthesized a valid binary `CONFIG.WL1` fixture with standard control mappings in a temporary directory and executed `raylib_wolf3d.exe -data <data> -nosound -savetest` with `APPDATA` and `LOCALAPPDATA` pointed to that directory. The process launched and completed `-savetest` with exit code `0`.
- **Known Test Limitations**:
  - The script asserts `if res.returncode != 0 or "ignoring invalid" in res.stdout`. However, `ReadConfigFile` (`WOLFSRC/WL_MAIN.C:151`) logs fallback warnings to **stderr**, not stdout.
  - Running `-savetest` exits immediately after save serialization testing and does not query or log active control mappings. Thus, step 5 verifies a **successful launch with a valid config fixture in a redirected environment**, but does not assert whether the fixture was parsed versus defaulting.
- **Unverified on Windows**:
  - Rejection of out-of-bounds scan codes and negative button indices was **not** tested in the Windows E2E harness.
  - In-game modification and reload of user settings was **not** tested in the Windows E2E harness.
  - Both boundary rejection and reload round-trips were verified on Linux under [`tests/persist_check.py`](../tests/persist_check.py) (WOLF-18/WOLF-7) and remain marked unverified natively on Windows.

---

## Interactive User Session Validation

- **Environment**: Interactive desktop console session (`WinSta0\Default`, Session 2).
- **Execution**: `chk\raylib_wolf3d-1.0.0-windows-x64\raylib_wolf3d.exe` (`d120a365`) launched with bundled runtime DLLs and shareware v1.4 assets.
- **Human Verification Feedback**: Evaluated and approved by project lead Raydelto Hernandez:
  - Full game loop, intro screens, sound effects, and OPL3 music playback confirmed.
  - Keyboard controls and mouse look/movement verified functional without lag or input drops.
  - Published as standalone Windows release on GitHub (`v1.0.0`).

---

## Architectural & Governance Compliance

1. **Raylib Quarantine**: `raylib.h` remains strictly quarantined to `port/id_raylib.c` (and test driver `tests/rl_keys_test.c`).
2. **Data Compliance**: Zero commercial or shareware data committed to git; external shareware v1.4 data verified with SHA-256.
3. **Cross-Platform Parity**: Linux build, XDG paths, and Ubuntu CI matrix preserved and intact.
