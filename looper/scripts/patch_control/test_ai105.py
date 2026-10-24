"""Host-only contract tests for the e4e1342 script boundary."""
import ast
import importlib.util
import pathlib
import re
import sys
import tempfile
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILDER = ROOT / "python/build_ai105.py"
BRIDGE = ROOT / "python/bridge_ai105.py"
POSTCONFIG = ROOT / "python/postconfigure_ai105.py"
CLIENT = ROOT / "scripts/patch_control/control_ai105.py"


class FakePart:
    def __init__(self, kind, registry):
        self.kind, self.registry = kind, registry
        self.name = kind
        self.values = {"mix0": 0, "mix1": 0}
        self.targets = []
        self.notes = []
        self.steps = []
        self.deleted = False
        self.x = self.y = 0

    def set_name(self, name):
        self.registry.pop(self.name, None)
        self.name = name
        self.registry[name] = self

    def set(self, path, value):
        self.values[path] = value
        if self.kind == "effectchain" and path == "spawn":
            slot = "mix0" if self.values["mix0"] == 0 else "mix1"
            self.values[slot] = 1
            effect = "distortion" if self.values["effect"] == 7 else "delay"
            self.registry[self.name + "~" + effect] = FakePart(effect, self.registry)

    def get(self, path):
        return self.values.get(path, 0)

    def set_target(self, *args):
        self.targets.append(args[-1].name if isinstance(args[-1], FakePart) else args[-1])

    def get_targets(self):
        return self.targets

    def set_position(self, x, y):
        self.x, self.y = x, y

    def get_position_x(self):
        return self.x

    def get_position_y(self):
        return self.y

    def add_note(self, *args):
        self.notes.append(args)

    def delete(self):
        self.deleted = True
        self.registry.pop(self.name, None)


class FakeDrum(FakePart):
    def set(self, step, row, velocity):
        self.steps.append((step, row, velocity))


class Contracts(unittest.TestCase):
    def setUp(self):
        # The first saved starter state used ai105_builder as the script name.
        self.registry = {name: FakePart(name, {}) for name in ("transport", "output", "ai105_builder")}
        self.tempo = 120
        self.me = types.SimpleNamespace(set=self.set_control, output=lambda _: None, highlight_line=lambda *_: None)
        self.mod = types.ModuleType("module")
        self.mod.create = self.create
        self.mod.get = self.registry.get
        self.mod.module = FakePart
        self.bespoke = types.ModuleType("bespoke")
        self.bespoke.get_modules = lambda: list(self.registry)
        self.bespoke.get_tempo = lambda: self.tempo
        self.drum = types.ModuleType("drumsequencer")
        self.drum.get = lambda path: types.SimpleNamespace(set=lambda *s: self.registry[path].steps.append(s)) if path in self.registry else None
        self.canvas = types.ModuleType("notecanvas")
        self.canvas.get = lambda path: self.registry.get(path)
        self.original = {n: sys.modules.get(n) for n in ("bespoke", "module", "drumsequencer", "notecanvas")}
        sys.modules.update(bespoke=self.bespoke, module=self.mod, drumsequencer=self.drum, notecanvas=self.canvas)

    def tearDown(self):
        for name, old in self.original.items():
            if old is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = old

    def create(self, kind, x, y):
        item = (FakeDrum if kind == "drumsequencer" else FakePart)(kind, self.registry)
        self.registry[kind] = item
        return item

    def set_control(self, path, value):
        if path == "transport~tempo":
            self.tempo = value

    def run_builder(self):
        exec(compile(self.instrument(BUILDER.read_text()), str(BUILDER), "exec"), {"me": self.me})

    @staticmethod
    def instrument(source):
        """Mirror ScriptModule::RunScript line instrumentation at e4e1342."""
        output = []
        previous = ""
        for index, line in enumerate(source.splitlines()):
            current = line.strip()
            prior = previous.strip()
            if (current and not current.startswith(("@", "#")) and
                    not prior.startswith("@") and not prior.endswith(",") and
                    not (current.endswith(":") and current.split()[0] in ("else:", "elif", "except"))):
                indent = line[:len(line) - len(line.lstrip(" "))]
                output.append(indent + f"me.highlight_line({index},0)")
            output.append(line)
            previous = line
        return "\n".join(output) + "\n"

    def test_build_routes_patterns_and_collision_guard(self):
        self.run_builder()
        self.assertEqual(self.tempo, 105)
        self.assertEqual(len(self.registry["ai105_drum_steps"].steps), 4 * (2 + 2 + 8))
        self.assertEqual(self.registry["ai105_drum_steps"].values["measures"], 4)
        self.assertEqual(self.registry["ai105_drum_steps"].values["step"], 7)
        self.assertEqual(len(self.registry["ai105_bass_notes"].notes), 32)
        self.assertEqual(self.registry["ai105_bass_split"].targets, ["ai105_bass_gain", "ai105_bass_looper"])
        self.assertEqual(self.registry["ai105_bass_looper"].values["write"], 0)
        self.assertEqual(self.registry["ai105_bass_looper"].values["num bars"], 8)
        self.assertEqual(self.registry["ai105_bass_notes"].values["play"], 1)
        self.assertEqual(self.registry["ai105_bass_fx~distortion"].values["type"], 3)
        self.assertEqual(self.registry["ai105_bass_fx~delay"].values["amount"], 0.25)
        count = len(self.registry)
        with self.assertRaisesRegex(RuntimeError, "already exist"):
            self.run_builder()
        self.assertEqual(len(self.registry), count)

    def test_both_scripts_compile_under_bespoke_instrumentation(self):
        for path in (BUILDER, BRIDGE, POSTCONFIG):
            compile(self.instrument(path.read_text()), str(path), "exec")

    def test_postconfigure_existing_patch(self):
        self.run_builder()
        exec(compile(self.instrument(POSTCONFIG.read_text()), str(POSTCONFIG), "exec"), {"me": self.me})
        self.assertEqual(self.registry["ai105_bass_notes"].values["play"], 1)
        self.assertEqual(self.registry["ai105_bass_fx~distortion"].values["type"], 3)
        self.assertEqual(self.registry["ai105_bass_fx~delay"].values["amount"], 0.25)
        self.assertEqual(self.registry["ai105_drum_voice"].x, 430)

    def test_client_rejects_stale_result_id(self):
        spec = importlib.util.spec_from_file_location("ai105_client_test", CLIENT)
        client = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(client)
        with tempfile.TemporaryDirectory() as tmp:
            result_dir = pathlib.Path(tmp) / "results"
            result_dir.mkdir()
            (result_dir / "used.json").write_text('{"ok":true}')
            with self.assertRaisesRegex(ValueError, "already has a result"):
                client.submit(tmp, {"version": 1, "id": "used", "op": "get_state"}, timeout=0)

    def test_bridge_rejects_bad_requests_and_checkpoints_gain(self):
        self.run_builder()
        tree = ast.parse(BRIDGE.read_text())
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
        with tempfile.TemporaryDirectory() as tmp:
            folder = pathlib.Path(tmp)
            (folder / "checkpoints").mkdir()
            env = {"json": __import__("json"), "os": __import__("os"), "re": re,
                   "bespoke": self.bespoke, "module": self.mod, "VERSION": 1,
                   "GAIN_PATHS": {"bass": "ai105_bass_gain", "drum": "ai105_drum_gain"},
                   "CHECKPOINTS": str(folder / "checkpoints")}
            exec(compile(ast.Module(body=functions, type_ignores=[]), str(BRIDGE), "exec"), env)
            handle = env["_handle"]
            for request in (
                {"version": 1, "id": "bad", "op": "eval", "value": "print(1)"},
                {"version": 1, "id": "bad", "op": "set_gain", "channel": "bass", "value": 0.8},
                {"version": 1, "id": "bad", "op": "set_gain", "channel": "bass", "value": True},
                {"version": 2, "id": "bad", "op": "get_state"},
            ):
                with self.assertRaises(ValueError):
                    handle(request)
            result = handle({"version": 1, "id": "change1", "op": "set_gain", "channel": "bass", "value": 0.3})
            self.assertTrue(result["ok"])
            self.assertEqual(result["before"], 0.2)
            self.assertTrue((folder / "checkpoints/change1.json").is_file())
            with self.assertRaisesRegex(ValueError, "already used"):
                handle({"version": 1, "id": "change1", "op": "set_gain", "channel": "bass", "value": 0.1})


if __name__ == "__main__":
    unittest.main()
