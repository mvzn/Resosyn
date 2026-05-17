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

| Trigger | Condition |
|---|---|
| Push | Any commit to `main` or `cmake-migration` |
| Pull request | Any PR targeting `main` |
| Manual | "Run workflow" button in the Actions tab |

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

## How to manually trigger a build

Useful for testing workflow changes without committing dummy code.

**Web UI:** Actions tab → "Build" workflow on the left → "Run workflow" dropdown → pick a branch → "Run workflow".

**CLI:**
```bash
gh workflow run build --ref cmake-migration
gh run watch                          # follow the latest run live
```

---

## How to download the built plugins

Wait for the workflow to finish (green check on all three jobs).

**Web UI:** click the workflow run → scroll to **Artifacts** at the bottom → click any to download as a zip.

You'll see 7 artifacts per successful run:
- `Resosyn-Linux-VST3`, `Resosyn-Linux-Standalone`
- `Resosyn-Windows-VST3`, `Resosyn-Windows-Standalone`
- `Resosyn-macOS-VST3`, `Resosyn-macOS-Standalone`, `Resosyn-macOS-AU`

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

First build per OS downloads JUCE (~2 min). Subsequent builds restore from cache (`build/_deps`) keyed on `runner.os` + JUCE tag. Bumping `GIT_TAG` in `CMakeLists.txt` invalidates the cache automatically.

If a cache becomes corrupted, delete it manually: Actions tab → **Caches** in the left sidebar.

---

## CI minute cost (private repos only)

- Linux: 1× minutes used (cheap)
- Windows: 2× minutes used
- macOS: 10× minutes used (the expensive one)

This repo is currently building on all three on every push. If minute usage becomes a concern, gate macOS behind tags:

```yaml
# in build.yml, under the matrix:
strategy:
  matrix:
    include:
      - os: ubuntu-latest
      - os: windows-latest
      - os: macos-latest
        if: github.event_name == 'workflow_dispatch' || startsWith(github.ref, 'refs/tags/')
```

Public repos: all OSes free, no limit.

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
