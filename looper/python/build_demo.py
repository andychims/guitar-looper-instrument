"""Run once in a Bespoke script module on a saved copy of the baseline patch.

Pinned to BespokeSynth e4e1342. This only creates ai105_* modules and connects
them to the existing output. The looper receives processed bass but stays idle.
"""

import bespoke
import drumsequencer
import module
import notecanvas

PREFIX = "ai105_"
REQUIRED = ("transport", "output")
PARTS = (("drum_steps", "drumsequencer", 40, 780),
    ("drum_voice", "drumsynth", 430, 780),
    ("drum_gain", "gain", 1190, 780),
    ("bass_notes", "notecanvas", 40, 1350),
    ("bass_voice", "oscillator", 480, 1350),
    ("bass_fx", "effectchain", 750, 1350),
    ("bass_split", "audiosplitter", 1430, 1350),
    ("bass_gain", "gain", 1600, 1300),
    ("bass_looper", "looper", 1600, 1530),
)


def build():
    paths = set(bespoke.get_modules())
    missing = [path for path in REQUIRED if path not in paths]
    if missing:
        raise RuntimeError("Required baseline module missing: " + ", ".join(missing))
    # Earlier starter layouts named the host script ai105_builder. That module
    # is not a generated patch part and must not trip the collision guard.
    occupied = [path for path in paths if path.startswith(PREFIX) and path != "ai105_builder"]
    if occupied:
        raise RuntimeError("ai105 modules already exist; open a fresh saved copy: " + ", ".join(sorted(occupied)))
    # Do not rerun against an existing generated patch; use the saved before
    # image to undo.
    created = []
    old_tempo = bespoke.get_tempo()
    try:
        parts = {}
        for key, kind, x, y in PARTS:
            item = module.create(kind, x, y)
            if item is None:
                raise RuntimeError("Could not create " + kind)
            created.append(item)
            item.set_name(PREFIX + key)
            parts[key] = item

        # The effect dropdown indices are from sorted EffectFactory registrations
        # at e4e1342: delay=6, distortion=7. Spawn order defines signal order.
        fx = parts["bass_fx"]
        fx.set("effect", 7)
        fx.set("spawn", 1)
        fx.set("effect", 6)
        fx.set("spawn", 1)
        if fx.get("mix0") < 0.99 or fx.get("mix1") < 0.99:
            raise RuntimeError("Native distortion/delay did not appear in bass effect chain")
        distortion = module.get(PREFIX + "bass_fx~distortion")
        delay = module.get(PREFIX + "bass_fx~delay")
        if distortion is None or delay is None:
            raise RuntimeError("Native effect child modules are unavailable")
        module.module.set(distortion, "type", 3)  # soft
        module.module.set(distortion, "clip", 0.8)
        module.module.set(distortion, "preamp", 2.0)
        module.module.set(delay, "amount", 0.25)  # wet echo; 8nd transport interval default

        drum = parts["drum_steps"]
        # The dynamic drumsequencer wrapper defines set(step,row,velocity),
        # shadowing IDrawableModule.set(control,value). Call the base binding
        # explicitly so grid resize is synchronous before writing 64 steps.
        module.module.set(drum, "measures", 4)  # source range 1..4
        module.module.set(drum, "step", 7)  # kInterval_16n
        # Drumsynth pads 0/1/2: kick, snare, hat. Other pad volumes default 0.
        voice = parts["drum_voice"]
        voice.set("vol0", 0.5)
        voice.set("vol1", 0.10)
        voice.set("noise1", 0.45)
        voice.set("vol2", 0.0)
        voice.set("noise2", 0.28)
        seq = drumsequencer.get(PREFIX + "drum_steps")
        if seq is None:
            raise RuntimeError("Created drum sequencer is unavailable to Python")
        for bar in range(4):
            first = bar * 16
            for step in (0, 8):
                seq.set(first + step, 0, 110)
            for step in (4, 12):
                seq.set(first + step, 1, 94)
            for step in range(0, 16, 2):
                seq.set(first + step, 2, 48)

        canvas_module = parts["bass_notes"]
        canvas_module.set("measures", 8)  # source range 1..16
        module.module.set(canvas_module, "play", 1)
        canvas = notecanvas.get(PREFIX + "bass_notes")
        if canvas is None:
            raise RuntimeError("Created note canvas is unavailable to Python")
        # A minor; absolute MIDI pitches. measurePos/length are in measures.
        bars = ((33, 40, 36, 40), (33, 40, 43, 40),
                (36, 43, 40, 43), (36, 43, 45, 43),
                (41, 36, 33, 36), (41, 45, 43, 36),
                (40, 43, 36, 33), (40, 43, 36, 31))
        for bar, pitches in enumerate(bars):
            for beat, pitch in enumerate(pitches):
                canvas.add_note(bar + beat / 4.0, pitch, 76 if beat else 96, 0.20)

        parts["drum_steps"].set_target(parts["drum_voice"])
        parts["drum_voice"].set_target(parts["drum_gain"])
        parts["drum_gain"].set_target("output")
        parts["bass_notes"].set_target(parts["bass_voice"])
        parts["bass_voice"].set_target(parts["bass_fx"])
        parts["bass_fx"].set_target(parts["bass_split"])
        parts["bass_split"].set_target(0, PREFIX + "bass_gain")
        parts["bass_split"].set_target(1, PREFIX + "bass_looper")
        parts["bass_gain"].set_target("output")
        parts["bass_looper"].set_target(parts["bass_gain"])

        # Keep headroom. Looper receives signal; write/record stays off.
        parts["drum_gain"].set("gain", 0.25)
        parts["bass_gain"].set("gain", 0.20)
        parts["bass_looper"].set("num bars", 8)
        parts["bass_looper"].set("write", 0)
        parts["bass_looper"].set("passthrough", 0)
        # No transport setter is bound. Use the verified UI control path.
        me.set("transport~tempo", 110)
        me.output("ai105 created: four-bar drums, eight-bar bass, native distortion+delay, idle processed-bass looper. Save As now.")
    except Exception:
        # Created modules are the only objects we delete. Existing audio and
        # module state are untouched unless the final tempo set was reached.
        for item in reversed(created):
            try:
                item.delete()
            except Exception:
                pass
        me.set("transport~tempo", old_tempo)
        raise


build()

# Feed generated music into the two clip recorders, with quiet live monitoring.
module.get("ai105_drum_gain").set_target("rhythm recorder")
module.get("ai105_bass_gain").set_target("lead recorder")
module.get("rhythm monitor").set("gain", 1)
module.get("lead monitor").set("gain", 1)
for path in bespoke.get_modules():
    if path.startswith("ai105_"):
        item = module.get(path)
        item.set_position(item.get_position_x(), item.get_position_y() + 1800)
module.get("demo_builder").set_position(30, 2300)
me.output("Demo ready: record Rhythm A for drums and Lead A for bass. Then silence the two monitors: the clips keep playing.")

# A post-loop echo makes the Lead FX shortcut audibly useful.
post_fx = module.get("lead FX")
post_fx.set("effect", 6)
post_fx.set("spawn", 1)
module.get("lead FX~delay").set("amount", 0.3)
