# Guitar looper starters for Bespoke Synth

This folder contains four optional layouts for the Bespoke source checkout: a two-track clip grid, the same grid with user-installed VST3 plug-ins, a script-based drum/bass starter, and a clip-demo scaffold with an idle script builder. They use stock audio modules plus the fork's `loopgrid` module. The tutorial includes a generated musical audio preview and screenshots. Native `.bsk` sessions, plug-in binaries, plug-in scan results, and machine preferences are not bundled.

Development is paused at a resumable prototype checkpoint. See [the illustrated tutorial](tutorial/tutorial.html) and its [portability notes](tutorial/README.md).

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

`launch.sh` runs `setup.py` first. Setup creates ignored files under `looper/runtime-data/`: a resolved copy of each layout, four preferences files, and copies of the Python scripts in `scripts/`. It keeps existing generated files unless called with `python3 looper/scripts/setup.py --force`. Run that command once after updating this package if older generated layouts or preferences exist. The preferences choose Bespoke's automatic output device, **no input device**, 48 kHz/256 frames, and 1× zoom; they hide the welcome screen and startup tooltips. The layout output limit is 0.1, and the basic/plugin test tones start at level 0.1. Keep system speaker volume low. To select a particular output, run setup with `--output-device 'Device Name' --force` before launch.

Choose a starter with `./looper/scripts/launch.sh basic`, `plugins`, `ai`, or `demo`. Native `.bsk` session saving remains an in-app action after you verify the layout. The JSON files are starting layouts, not saved performances.

## Basic grid

Two tracks, Rhythm and Lead, each have two stock loopers. The quiet test sources feed dedicated stock recorders; each recorder's live monitor passes through its track gain to output. Loop playback runs through each track's effect chain and the same track gain into the output. The 650×340 panel opens prominently at the upper left. Click an empty **Rhythm A/B** or **Lead A/B** cell to arm capture for the next bar, then record for the selected **4 or 8 bars**. Commit and launch can take an additional bar or more. Click a playing cell to queue a stop; click a stopped cell to queue a launch. Click again to cancel a pending stop or launch. One clip per track plays, and the tracks are independent. The grid binds the recorder's special A/B looper cables from saved slot names during initialization; inspect those cables and the normal audio routes in Bespoke before recording. Keep playback out of recorder inputs to avoid feedback. Disable the test sources when moving to another source.

Each row has **gain**, **mute**, **solo**, **FX**, and **mon**. Gain, mute, and solo act through the mapped track gain module; a track stays muted when solo changes. FX toggles the mapped stock EffectChain. Mon silences/restores the separate live-monitor gain without changing what the recorder captures. **Stop all** queues playback stops on the next bar and cancels or aborts captures; **Panic** immediately silences the loopers and mutes both tracks. **120 BPM** changes tempo only while all cells are empty or stopped. The layout does not save transport tempo.

**Performance keys** is off by default. Enable it in the panel, then use the exact **Ctrl+Option** chord with the keys below. The key map is editable: click **Map key**, click a panel control, then press Ctrl+Option plus a letter or digit. Duplicate keys, other modifiers, and navigation keys are rejected. Save a native session to persist changes.

| Action | Ctrl+Option key |
|---|---|
| Rhythm A/B, Lead A/B | 1 / 2, 3 / 4 |
| Stop all, Panic, 120 BPM | S, P, T |
| Rhythm/Lead mute | Q / W |
| Rhythm/Lead solo | A / D |
| Rhythm/Lead FX | E / R |
| Rhythm/Lead monitor | Z / X |
| Rhythm/Lead gain +0.1 | U / I |

For MIDI, configure and connect a native Bespoke `midicontroller` first. Click **Map MIDI**, click a panel control, then send a MIDI note or CC. Buttons accept notes or momentary CC; the gain sliders require CC. The mapping is tied to that controller and its page, so reconnect it and save a native session to persist the map. A CoreMIDI virtual source verified note and CC learning, same-controller remapping, continuous gain, and saved mapping/labels; physical controller use and a fresh post-reload trigger check remain open. Mapping does not create a controller or configure a hardware port. The panel's four cell control paths are `grid~Rhythm A`, `grid~Rhythm B`, `grid~Lead A`, and `grid~Lead B`.

The grid's occupancy markers track its own captures; direct edits to stock loopers can leave them stale. The stock recorder has a 60-second rolling buffer; requests over 59 seconds are ignored. A captured slot cannot be re-recorded or cleared from the grid. The fork wraps negative commit indices at the loop boundary; the focused commit-math test covers that arithmetic. The recorder's commit path can copy large buffers and may allocate when a new audio channel is first used, so dropout-free real-time behavior during first capture is unverified. The grid is not a sample-accurate Session View.

## Optional plug-in grid

Install and scan Surge XT, Surge XT Effects, BYOD, and CHOWTapeModel separately. Their binaries and factory content are not distributed here. `setup.py` expands `@USER_VST3@` in the public layout to the current user's `~/Library/Audio/Plug-Ins/VST3`; `launch.sh plugins` stops if any of the four bundles is absent. Bespoke must also have those VST3 descriptors in its known-plugin list; installation alone is insufficient.

The Rhythm test tone passes through BYOD before capture, the Lead tone passes through CHOWTapeModel before capture, and recorded Rhythm loops pass through Surge XT Effects. The `computer keyboard` module sends notes to Surge XT, whose audio reaches the Lead recorder. Hover the computer keyboard and press **A**, **W**, or **S** to play in the default Ableton QWERTY mapping; **Z/X** shift octaves. Click away from a text field first. Disable the Lead test tone to hear Surge alone. The plug-in modules start minimized and sit in spaced rows; pan or zoom to inspect their cables. Verify VST3 load, audible processing, and native save/reopen in Bespoke before relying on a saved performance.

The panel's Rhythm FX button bypasses only its mapped stock EffectChain; the extra Surge XT Effects plug-in is downstream and stays active. Use that plug-in's own enabled control if you need to bypass it.

## Clip demo scaffold

`./looper/scripts/launch.sh demo` opens a quiet performance layout with four empty clips and an idle `demo_builder` script module. Setup copies `looper/python/build_demo.py` to `looper/runtime-data/scripts/build_demo.py`. Save a fresh native baseline, then load that script into the module and run it once after reviewing Bespoke's script trust prompt. The builder creates a drum sequence and bass line, routes them into the Rhythm and Lead recorders, turns on the two monitors, and adds a post-loop Lead delay. It does **not** record or start a clip for you: use the panel to capture Rhythm A and Lead A, then switch off the monitors to hear just the captured loops. Do not run the builder twice on the same patch; open the saved baseline to start over.

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

`scripts/test.sh` runs the copied host-side AI script contracts and compiles/runs the pure `LoopGridState`, `LooperCommitMath`, and `PerformanceBindings` tests against fork source. The optional `tools/plugin_probe/` contains JUCE VST3 scan and signal/state test source; its build output and generated WAVs belong in ignored runtime data. Headless probe success does not replace Bespoke's own UI, audio routing, or session test.

On the same macOS toolchain, build and run the optional probe with:

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk cmake -S looper/tools/plugin_probe -B looper/tools/plugin_probe/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk '-DCMAKE_CXX_COMPILER_LAUNCHER=/usr/bin/arch;-arm64' '-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/arch;-arm64'
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk cmake --build looper/tools/plugin_probe/build --parallel 4
python3 looper/tools/plugin_probe/run_all.py
```

Each plug-in runs in its own process with a timeout. The probe writes a JUCE known-plugin XML, per-plugin results, and short generated WAVs under ignored `looper/runtime-data/plugin-probe/`; review those outputs before importing any descriptors into Bespoke.
