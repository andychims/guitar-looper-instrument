#!/bin/bash
set -euo pipefail
looper_root="$(cd "$(dirname "$0")/.." && pwd)"
repo_root="$(cd "$looper_root/.." && pwd)"
mode="${1:-basic}"
case "$mode" in
  basic|plugins|ai) ;;
  *) echo "Usage: $0 [basic|plugins|ai]" >&2; exit 2 ;;
esac
python3 "$looper_root/scripts/setup.py"
if [[ "$mode" == plugins ]]; then
  vst_dir="$HOME/Library/Audio/Plug-Ins/VST3"
  for bundle in 'Surge XT.vst3' 'Surge XT Effects.vst3' 'BYOD.vst3' 'CHOWTapeModel.vst3'; do
    if [[ ! -d "$vst_dir/$bundle" ]]; then
      echo "Missing optional plug-in: $vst_dir/$bundle" >&2
      exit 1
    fi
  done
fi
binary="$repo_root/ignore/build-intel/Source/BespokeSynth_artefacts/Release/BespokeSynth.app/Contents/MacOS/BespokeSynth"
if [[ ! -x "$binary" ]]; then
  echo "Source binary missing. Run $looper_root/scripts/build-macos.sh first." >&2
  exit 1
fi
export BESPOKE_DATA_DIR="$looper_root/runtime-data"
exec "$binary" "$BESPOKE_DATA_DIR/prefs/$mode.json"
