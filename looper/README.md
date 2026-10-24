# Guitar looper starters for Bespoke Synth

This folder contains three optional layouts for the Bespoke source checkout: a two-track clip grid, the same grid with user-installed VST3 plug-ins, and a script-based drum/bass starter. They use stock audio modules plus the fork's `loopgrid` module. They do not include captured audio, native `.bsk` sessions, plug-in binaries, plug-in scan results, or machine preferences.

See [VALIDATION.md](VALIDATION.md) for the current local test results and remaining acceptance work.

## Quick start on macOS Apple Silicon

From a fresh fork checkout, initialize its submodules, then:

```sh
git submodule update --init --recursive
./looper/scripts/build-macos.sh
./looper/scripts/test.sh
./looper/scripts/launch.sh basic
```

The build script follows the tested x86_64 Rosetta recipe with an arm64 compiler process and a macOS 14.5 SDK. It checks that SDK, CMake, JUCE, and Apple Silicon are present; it does not install prerequisites. Bespoke's own source build also needs its normal dependencies, including compatible Python. If the SDK lives elsewhere, set `BESPOKE_SDKROOT` to its path. If CMake cannot find your compatible Python, set `BESPOKE_PYTHON_ROOT` to its installation root. Build failures should be addressed using the checkout's upstream build instructions and CMake diagnostic output.

`launch.sh` runs `setup.py` first. Setup creates ignored files under `looper/runtime-data/`: a resolved copy of each layout, three preferences files, and copies of the AI Python scripts in `scripts/`. It keeps existing generated files unless called with `python3 looper/scripts/setup.py --force`. The preferences choose Bespoke's automatic output device, **no input device**, 48 kHz/256 frames, and hide the welcome screen and startup tooltips. The layout output limit is 0.1, and the basic/plugin test tones start at level 0.1. Keep system speaker volume low. To select a particular output, run setup with `--output-device 'Device Name' --force` before launch.

Choose a starter with `./looper/scripts/launch.sh basic`, `plugins`, or `ai`. Native `.bsk` session saving remains an in-app action after you verify the layout. The JSON files are starting layouts, not saved performances.

## Basic grid

Two tracks, Rhythm and Lead, each have two stock loopers. The quiet test sources feed dedicated stock recorders; each recorder's live monitor passes through its track gain to output. Loop playback runs through each track's effect chain and the same track gain into the output. The grid queues 4- or 8-bar captures and bar-aligned starts. It binds the recorder's special A/B looper cables from saved slot names during initialization; inspect those cables and the normal audio routes in Bespoke before recording. The grid has per-track mute, solo, and gain controls; solo selects at most one track and mute remains effective through solo changes. Keep playback out of recorder inputs to avoid feedback. Disable the test sources when moving to another source.

The grid's occupancy markers track its own captures; direct edits to stock loopers can leave them stale. The stock recorder has a 60-second rolling buffer; requests over 59 seconds are ignored. A captured slot cannot be re-recorded or cleared from the grid. The fork wraps negative commit indices at the loop boundary; the focused commit-math test covers that arithmetic. The recorder's commit path can copy large buffers and may allocate when a new audio channel is first used, so dropout-free real-time behavior during first capture is unverified. The grid is not a sample-accurate Session View.

## Optional plug-in grid

Install and scan Surge XT, Surge XT Effects, BYOD, and CHOWTapeModel separately. Their binaries and factory content are not distributed here. `setup.py` expands `@USER_VST3@` in the public layout to the current user's `~/Library/Audio/Plug-Ins/VST3`; `launch.sh plugins` stops if any of the four bundles is absent. Bespoke must also have those VST3 descriptors in its known-plugin list; installation alone is insufficient.

The Rhythm test tone passes through BYOD before capture, the Lead tone passes through CHOWTapeModel before capture, and recorded Rhythm loops pass through Surge XT Effects. The `computer keyboard` module sends notes to Surge XT, whose audio reaches the Lead recorder. Hover the computer keyboard and press **A**, **W**, or **S** to play in the default Ableton QWERTY mapping; **Z/X** shift octaves. Click away from a text field first. Disable the Lead test tone to hear Surge alone. The plug-in modules start minimized and sit in spaced rows; pan or zoom to inspect their cables. Verify VST3 load, audible processing, and native save/reopen in Bespoke before relying on a saved performance.

## Console checks

Press the backtick key to open Bespoke's console, type a command, then press Return. The current fork source defines these local commands; test their effect in the running build before using them as performance controls:

```text
tempo 120
control rhythm source~vol 0
control lead source~vol 0
inspect
```

`control` looks up a full `module~control` path and applies a finite numeric value through the normal UI control setter. Spaces in the path are accepted. The two examples silence only the generated test tones; use the modules' visible controls to restore a level of 0.1 or less. `inspect` writes `looper/runtime-data/inspect.json` with tempo, module metadata, and current UI control values. It omits audio buffers and plug-in state blobs, but can contain local plug-in file paths, so keep it out of published artifacts. It is a point-in-time inspection file, not a session backup or audio proof. `tempo` changes the running transport; the starter JSON does not save tempo. Save a native `.bsk` after checking the result.

## AI drum/bass starter

`./looper/scripts/launch.sh ai` opens a starter layout with transport, scale, output, and an idle script module. The source `build_ai105.py`, `postconfigure_ai105.py`, and optional `bridge_ai105.py` are copied to `looper/runtime-data/scripts/` by setup. The builder targets the Bespoke source API at commit `e4e1342` and generates stock drums, bass, effects, and an idle looper. Load and run it from the Bespoke script module only after saving a fresh native baseline; review the script before accepting Bespoke's script trust prompt, then inspect the generated route and save again. The optional file bridge accepts a small typed local request file for state inspection or bounded gain changes. It opens no network listener. Do not run it unless you need that control path. `scripts/patch_control/control_ai105.py` is its local client.

## Tests and probe

`scripts/test.sh` runs the copied host-side AI script contracts and compiles/runs the pure `LoopGridState` and `LooperCommitMath` tests against fork source headers. The optional `tools/plugin_probe/` contains JUCE VST3 scan and signal/state test source; its build output and generated WAVs belong in ignored runtime data. Headless probe success does not replace Bespoke's own UI, audio routing, or session test.

On the same macOS toolchain, build and run the optional probe with:

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk cmake -S looper/tools/plugin_probe -B looper/tools/plugin_probe/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk '-DCMAKE_CXX_COMPILER_LAUNCHER=/usr/bin/arch;-arm64' '-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/arch;-arm64'
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk cmake --build looper/tools/plugin_probe/build --parallel 4
python3 looper/tools/plugin_probe/run_all.py
```

Each plug-in runs in its own process with a timeout. The probe writes a JUCE known-plugin XML, per-plugin results, and short generated WAVs under ignored `looper/runtime-data/plugin-probe/`; review those outputs before importing any descriptors into Bespoke.
