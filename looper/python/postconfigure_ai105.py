"""Run once in the existing saved ai105 patch to enable bass and set native FX.

This changes only ai105 controls and ai105 module positions. Save As afterward.
"""
import module

PREFIX = "ai105_"
POSITIONS = (("drum_steps", 40, 780),
    ("drum_voice", 430, 780),
    ("drum_gain", 1190, 780),
    ("bass_notes", 40, 1350),
    ("bass_voice", 480, 1350),
    ("bass_fx", 750, 1350),
    ("bass_split", 1430, 1350),
    ("bass_gain", 1600, 1300),
    ("bass_looper", 1600, 1530))
CONTROLS = (("bass_notes", "play", 1),
    ("bass_fx~distortion", "type", 3),
    ("bass_fx~distortion", "clip", 0.8),
    ("bass_fx~distortion", "preamp", 2.0),
    ("bass_fx~delay", "amount", 0.25))


def configure():
    objects = {}
    for key, _, _ in POSITIONS:
        objects[key] = module.get(PREFIX + key)
        if objects[key] is None:
            raise RuntimeError("Missing ai105 module: " + key)
    for key, _, _ in CONTROLS:
        if key not in objects:
            objects[key] = module.get(PREFIX + key)
        if objects[key] is None:
            raise RuntimeError("Missing ai105 effect child: " + key)
    old_positions = [(objects[key], objects[key].get_position_x(), objects[key].get_position_y()) for key, _, _ in POSITIONS]
    old_controls = [(objects[key], control, module.module.get(objects[key], control)) for key, control, _ in CONTROLS]
    try:
        for key, control, value in CONTROLS:
            module.module.set(objects[key], control, value)
            actual = module.module.get(objects[key], control)
            if abs(actual - value) > 0.01:
                raise RuntimeError("Control did not update: " + key + "~" + control)
        for key, x, y in POSITIONS:
            objects[key].set_position(x, y)
    except Exception:
        for item, control, old in reversed(old_controls):
            module.module.set(item, control, old)
        for item, x, y in old_positions:
            item.set_position(x, y)
        raise
    me.output("ai105 bass play enabled, soft distortion and dotted-eighth delay set, modules spaced. Save As now.")


configure()
