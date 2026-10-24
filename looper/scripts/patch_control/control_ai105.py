#!/usr/bin/env python3
"""Typed local client for the optional Bespoke ai105 file bridge."""
import argparse
import json
import os
from pathlib import Path
import time
import uuid

DEFAULT_DIR = Path(os.environ.get("BESPOKE_DATA_DIR", str(Path(__file__).resolve().parents[2] / "runtime-data"))) / "ai105-control"


def submit(directory, request, timeout=5.0):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    pending = directory / "request.json"
    ident = request["id"]
    result = directory / "results" / (ident + ".json")
    if result.exists():
        raise ValueError("request id already has a result")
    temp = directory / ("." + ident + ".tmp")
    try:
        with temp.open("x", encoding="utf-8") as handle:
            json.dump(request, handle, separators=(",", ":"))
        try:
            os.link(temp, pending)  # fails if another request is pending
        except FileExistsError as error:
            raise RuntimeError("bridge has a pending request") from error
    finally:
        temp.unlink(missing_ok=True)
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        if result.is_file():
            return json.loads(result.read_text(encoding="utf-8"))
        time.sleep(0.05)
    raise TimeoutError("No bridge result; confirm bridge_ai105.py is running in Bespoke")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", type=Path, default=DEFAULT_DIR)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inspect")
    gain = commands.add_parser("set-gain")
    gain.add_argument("channel", choices=("bass", "drum"))
    gain.add_argument("value", type=float)
    args = parser.parse_args()
    request = {"version": 1, "id": uuid.uuid4().hex, "op": "get_state"}
    if args.command == "set-gain":
        if not 0.0 <= args.value <= 0.5:
            parser.error("gain must be in [0, 0.5]")
        request.update(op="set_gain", channel=args.channel, value=args.value)
    response = submit(args.dir, request)
    print(json.dumps(response, indent=2, sort_keys=True))
    if not response.get("ok"):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
