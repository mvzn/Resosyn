# Resosyn CI — How to use it

The `build` workflow in `.github/workflows/build.yml` compiles Resosyn on Linux, Windows, and macOS in parallel and uploads the resulting plugins + build logs as downloadable artifacts.

---

## What it builds

| OS | Plugin formats | Standalone |
|---|---|---|
| Linux (ubuntu-latest) | VST3 (`.so` in `.vst3` bundle) | Native binary |
| Windows (windows-latest) | VST3 (`.vst3` folder) | `.exe` |
| macOS (macos-latest) | VST3 + AU (`.component`) | `.app` |

Universal macOS binary (x86_64 + arm64). Release build only.

---

## When it runs

| Trigger | Linux | Windows | macOS |
|---|---|---|---|
| Push to `main` / `cmake-migration` | ✓ | — | — |
| Pull request targeting `main` | ✓ | — | — |
| Manual ("Run workflow" button) | ✓ | ✓ | ✓ |
| Manual with **Linux only** checked | ✓ | — | — |

**Why this split:** Windows costs 2× and macOS costs 10× Linux minutes on private repos. Auto-builds stay cheap; cross-platform builds happen on demand.

Rapid pushes auto-cancel earlier in-flight runs (`concurrency` block). Add `[skip ci]` to a commit message to skip CI entirely.

---

## How to push and watch a build

```bash
git push                              # triggers if branch matches the list above
gh workflow view build --web          # open Actions tab in browser (gh CLI)
```

Or in the GitHub UI: repo → **Actions** tab → click the latest run.

You'll see three parallel jobs (`Linux`, `Windows`, `macOS`). Click any one to watch logs stream live.

---

## How to manually trigger a cross-platform build

This is the only way to build for Windows and macOS — push and PR triggers run Linux only to save CI minutes.

**Web UI:** Actions tab → "Build" workflow on the left → "Run workflow" dropdown → pick a branch → leave **"Linux only"** unchecked → "Run workflow".

To rebuild only Linux (e.g., testing workflow YAML changes), check the "Linux only" box.

**CLI:**
```bash
gh workflow run build --ref cmake-migration                  # all three OSes
gh workflow run build --ref cmake-migration -f linux_only=true  # Linux only
gh run watch                                                  # follow the latest run live
```

---

## How to download the built plugins

Wait for the workflow to finish (green check on every job in the run).

**Web UI:** click the workflow run → scroll to **Artifacts** at the bottom → click any to download as a zip.

**Artifact count depends on what ran:**
- Push / PR / "Linux only" manual: 2 plugin artifacts (`Resosyn-Linux-VST3`, `Resosyn-Linux-Standalone`)
- Full manual cross-platform run: 7 plugin artifacts:
  - `Resosyn-Linux-VST3`, `Resosyn-Linux-Standalone`
  - `Resosyn-Windows-VST3`, `Resosyn-Windows-Standalone`
  - `Resosyn-macOS-VST3`, `Resosyn-macOS-Standalone`, `Resosyn-macOS-AU`

Plus one `build-log-<OS>-<run_number>` log artifact per OS that ran (see next section).

**CLI:**
```bash
gh run download                       # interactive picker for the latest run
gh run download <run-id> -n Resosyn-Linux-VST3   # specific artifact from specific run
gh run download <run-id> -p "Resosyn-*"          # all binary artifacts
```

**Installing locally:** unzip and copy into the host OS's plugin folder:
- Linux: `~/.vst3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS VST3: `~/Library/Audio/Plug-Ins/VST3/`
- macOS AU: `~/Library/Audio/Plug-Ins/Components/`

Unsigned macOS binaries trigger a Gatekeeper warning on first load — right-click → Open the first time, or `xattr -dr com.apple.quarantine path/to/Resosyn.vst3`.

---

## How to download build logs

Every push uploads a separate log artifact per OS:
- `build-log-Linux-<run_number>`
- `build-log-Windows-<run_number>`
- `build-log-macOS-<run_number>`

Each contains a `.log` file with the full CMake configure + build output plus a header (commit SHA, OS, build type, timestamp). Filename includes `github.sha` for long-term correlation.

**Critical:** logs are uploaded even when the build fails (`if: always()`) — this is the first place to look for CI-only failures.

```bash
gh run download <run-id> -p "build-log-*"   # all three OSes' logs
```

Retention: 30 days.

---

## Debugging a failed build

1. **Read the log artifact** for the failing OS — `gh run download <run-id> -n build-log-<OS>-<run-num>`. Error is usually in the last 30 lines.

2. **Re-run only failed jobs:** Actions tab → failed run → "Re-run failed jobs" dropdown. Saves time when only one platform broke.

3. **Enable verbose logging:** "Re-run jobs" → "Enable debug logging". Sets `ACTIONS_STEP_DEBUG=true`. Voluminous but useful for setup issues.

4. **Reproduce locally** (Linux only without a Mac/Windows machine):
   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```
   Match the CI deps with: `sudo apt-get install libasound2-dev libfreetype-dev libfontconfig1-dev libgtk-3-dev libxinerama-dev libxrandr-dev libxcursor-dev libxcomposite-dev mesa-common-dev ninja-build`

5. **Test workflow YAML changes without polluting commit history:** push to a feature branch, use `workflow_dispatch` (manual trigger) to iterate. Combined with the log artifacts, you can debug a Windows-only failure from Linux.

---

## Caching

Two independent caches per OS:

1. **JUCE source cache** (`build/_deps`) — keyed on `runner.os` + JUCE tag. Saves the ~2 min git clone. Bumping `GIT_TAG` in `CMakeLists.txt` invalidates automatically.

2. **sccache (compiler object cache)** — wraps `cc`/`c++`/`cl.exe` and caches compiled objects by input hash. JUCE module sources don't change between your pushes, so they cache-hit fully. Expected savings on a typical "edit a few files" push: ~80–90% of compile time (only your changed `.cpp` files recompile). Max 500 MB per OS; LRU-evicted by GitHub after 7 days of no access.

Cache hit rate is logged at the end of every build artifact log (`--- sccache stats ---` section). Look for `Cache hits: N / M`. First-run-after-cache-flush hits 0; steady-state usually 80%+.

If a cache becomes corrupted, delete it manually: Actions tab → **Caches** in the left sidebar.

---

## CI minute cost (private repos only)

| OS | Multiplier |
|---|---|
| Linux | 1× |
| Windows | 2× |
| macOS | 10× |

**Default behavior is already cost-optimized:** push/PR runs Linux only. Windows and macOS only consume minutes when you manually trigger a build.

If you want to ALSO auto-build all three on tag pushes (e.g., for releases), add a tag trigger:

```yaml
on:
  push:
    branches: [main]
    tags: ['v*']    # also runs on v1.0.0, v1.2.3, etc.
```

…and change the matrix expression to include tags:

```yaml
os: ${{ fromJSON((github.event_name != 'workflow_dispatch' && !startsWith(github.ref, 'refs/tags/') || inputs.linux_only) && '["ubuntu-latest"]' || '["ubuntu-latest", "windows-latest", "macos-latest"]') }}
```

Public repos: all OSes free, no limit — but the manual-only gate still saves wall-clock time.

---

## Modifying the workflow

The workflow file is `.github/workflows/build.yml`. Common edits:

| Change | Where |
|---|---|
| Add a new trigger branch | `on.push.branches` |
| Add a Debug build | Add to `matrix.build_type` |
| Pin a Linux version | `matrix.os: [ubuntu-22.04, ...]` |
| Change JUCE version | `GIT_TAG` in `CMakeLists.txt` (not the workflow) |
| Add a code signing step | New step after Build, gated `if: runner.os == 'macOS'`, secrets at repo settings |
| Trim log retention | `retention-days` on the upload step |

After editing, push and watch the next run. If the YAML is malformed, the Actions tab will show a parse error before any job starts.
