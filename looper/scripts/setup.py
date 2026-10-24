#!/usr/bin/env python3
"""Create local, ignored Bespoke looper preferences and script copies."""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'runtime-data'
PRESETS = {'basic': 'basic-grid.json', 'plugins': 'plugin-grid.json', 'ai': 'ai-starter.json'}
VST_MARKER = '@USER_VST3@'


def write_unless_present(path, payload, force):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not force:
        print(f'kept {path}')
        return
    path.write_bytes(payload)
    print(f'wrote {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--force', action='store_true', help='replace generated local files')
    parser.add_argument('--output-device', default='auto', help='Bespoke output device name; default is auto')
    args = parser.parse_args()
    user_vst3 = Path.home() / 'Library/Audio/Plug-Ins/VST3'
    for mode, filename in PRESETS.items():
        source = json.loads((ROOT / 'presets' / filename).read_text())
        for module in source['modules']:
            if module['type'] == 'plugin':
                plugin = module['vst']
                if not plugin.startswith(VST_MARKER + '/'):
                    raise ValueError(f'plugin path is not portable: {plugin}')
                module['vst'] = str(user_vst3 / plugin.split('/', 1)[1])
            if module['type'] == 'output' and module.get('limit', 1) > 0.1:
                raise ValueError(f'output limit exceeds 0.1 in {filename}')
        layout = DATA / 'layouts' / filename
        write_unless_present(layout, (json.dumps(source, indent=2) + '\n').encode(), args.force)
        prefs = {
            'audio_output_device': args.output_device,
            'audio_input_device': 'none',
            'samplerate': 48000,
            'buffersize': 256,
            'width': 1440 if mode == 'plugins' else 1200,
            'height': 900 if mode == 'plugins' else 760,
            'zoom': 0.6 if mode == 'plugins' else (1.0 if mode == 'ai' else 0.75),
            'ui_scale': 1.0,
            'show_welcome_screen': False,
            'show_tooltips_on_load': False,
            'layout': str(layout),
        }
        write_unless_present(DATA / 'prefs' / (mode + '.json'),
                             (json.dumps(prefs, indent=2) + '\n').encode(), args.force)
    for script in sorted((ROOT / 'python').glob('*.py')):
        write_unless_present(DATA / 'scripts' / script.name, script.read_bytes(), args.force)


if __name__ == '__main__':
    main()
