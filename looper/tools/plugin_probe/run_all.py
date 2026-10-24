#!/usr/bin/env python3
"""Run each installed VST3 in an isolated, timed JUCE probe process."""

import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
EXE = HERE / 'build/plugin_probe_artefacts/Release/plugin_probe'
OUTPUT = ROOT / 'runtime-data/plugin-probe'
OUTPUT.mkdir(parents=True, exist_ok=True)
NAMES = ['Surge XT', 'Surge XT Effects', 'BYOD', 'CHOWTapeModel']
combined = ET.Element('KNOWNPLUGINS')
results = {}

for name in NAMES:
    path = Path.home() / 'Library/Audio/Plug-Ins/VST3' / (name + '.vst3')
    slug = name.lower().replace(' ', '-')
    item = {'path': str(path), 'exists': path.is_dir()}
    results[name] = item
    if not item['exists']:
        continue
    for stage, extension, limit in [('scan', '.xml', 30), ('run', '.wav', 60)]:
        dest = (OUTPUT / (slug + '-scan.xml')) if stage == 'scan' else (OUTPUT / (slug + '.wav'))
        try:
            done = subprocess.run([str(EXE), stage, str(path), str(dest)],
                                  capture_output=True, text=True, timeout=limit)
            item[stage] = {'returncode': done.returncode, 'stdout': done.stdout,
                           'stderr': done.stderr, 'output': str(dest) if dest.exists() else None}
            if stage == 'scan' and done.returncode == 0:
                for plugin in ET.parse(dest).getroot().findall('PLUGIN'):
                    combined.append(plugin)
        except subprocess.TimeoutExpired as exc:
            item[stage] = {'timeout_seconds': limit, 'stdout': (exc.stdout or b'').decode(errors='replace') if isinstance(exc.stdout, bytes) else exc.stdout,
                           'stderr': (exc.stderr or b'').decode(errors='replace') if isinstance(exc.stderr, bytes) else exc.stderr}
        if stage == 'scan' and item[stage].get('returncode') != 0:
            break

ET.indent(combined)
ET.ElementTree(combined).write(OUTPUT / 'scan-results.xml', encoding='utf-8', xml_declaration=True)
(OUTPUT / 'PLUGIN_PROBE_RESULTS.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps({name: {stage: entry.get(stage, {}).get('returncode', entry.get(stage, {}).get('timeout_seconds'))
                         for stage in ('scan', 'run')} for name, entry in results.items()}, indent=2))
if any(item.get(stage, {}).get('returncode') != 0 for item in results.values() for stage in ('scan', 'run')):
    sys.exit(1)
