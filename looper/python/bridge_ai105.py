"""Optional file-based local control for the saved ai105 patch (Bespoke e4e1342).

Run once in a SECOND script module. Polling uses ScriptModule::Poll on the main
thread; it never opens a socket or evaluates request text as Python.
"""
import json
import os
import re

import bespoke
import module

DIR = os.path.join(os.environ.get("BESPOKE_DATA_DIR", os.path.expanduser("~/Documents/Bespoke Synth")), "ai105-control")
REQUEST = os.path.join(DIR, "request.json")
RESULTS = os.path.join(DIR, "results")
CHECKPOINTS = os.path.join(DIR, "checkpoints")
VERSION = 1
GAIN_PATHS = {"drum": "ai105_drum_gain", "bass": "ai105_bass_gain"}

for folder in (DIR, RESULTS, CHECKPOINTS):
    os.makedirs(folder, exist_ok=True)


def _atomic_json(path, data):
    temp = path + ".tmp"
    with open(temp, "w") as handle:
        json.dump(data, handle, sort_keys=True)
    os.replace(temp, path)


def _state():
    paths = set(bespoke.get_modules())
    names = ["ai105_drum_steps", "ai105_drum_voice", "ai105_drum_gain",
             "ai105_bass_notes", "ai105_bass_voice", "ai105_bass_fx",
             "ai105_bass_split", "ai105_bass_gain", "ai105_bass_looper"]
    return {"version": VERSION, "tempo": bespoke.get_tempo(),
            "modules": {name: module.get(name).get_targets() for name in names if name in paths},
            "gains": {key: module.get(path).get("gain") for key, path in GAIN_PATHS.items() if path in paths}}


def _handle(req):
    if type(req) is not dict or set(req) - {"version", "id", "op", "channel", "value"}:
        raise ValueError("invalid fields")
    if req.get("version") != VERSION:
        raise ValueError("unsupported version")
    ident = req.get("id")
    if type(ident) is not str or not re.fullmatch(r"[A-Za-z0-9_-]{1,48}", ident):
        raise ValueError("invalid id")
    op = req.get("op")
    if op == "get_state":
        if set(req) != {"version", "id", "op"}:
            raise ValueError("get_state accepts no extra fields")
        return {"id": ident, "ok": True, "state": _state()}
    if op != "set_gain" or set(req) != {"version", "id", "op", "channel", "value"}:
        raise ValueError("operation not allowed")
    channel = req["channel"]
    if type(channel) is not str or channel not in GAIN_PATHS:
        raise ValueError("invalid channel")
    value = req["value"]
    if type(value) not in (int, float) or not (0.0 <= value <= 0.5):
        raise ValueError("gain must be numeric in [0, 0.5]")
    path = GAIN_PATHS[channel]
    if path not in bespoke.get_modules():
        raise ValueError("ai105 gain module missing")
    target = module.get(path)
    old = target.get("gain")
    checkpoint = os.path.join(CHECKPOINTS, ident + ".json")
    if os.path.exists(checkpoint):
        raise ValueError("request id already used")
    _atomic_json(checkpoint, {"id": ident, "path": path, "control": "gain", "before": old})
    target.set("gain", float(value))
    return {"id": ident, "ok": True, "before": old, "after": target.get("gain"), "checkpoint": checkpoint}


def poll_ai105():
    try:
        if os.path.isfile(REQUEST):
            with open(REQUEST, "rb") as handle:
                raw = handle.read(2049)
            os.unlink(REQUEST)
            ident = "invalid"
            try:
                if len(raw) > 2048:
                    raise ValueError("request too large")
                req = json.loads(raw.decode("utf-8"))
                if type(req) is dict and type(req.get("id")) is str and re.fullmatch(r"[A-Za-z0-9_-]{1,48}", req["id"]):
                    ident = req["id"]
                result = _handle(req)
            except Exception as error:
                result = {"id": ident, "ok": False, "error": str(error)}
            _atomic_json(os.path.join(RESULTS, ident + ".json"), result)
    except Exception as error:
        me.output("ai105 bridge polling error: " + str(error))
    me.schedule_call(0.25, "poll_ai105()")


poll_ai105()
