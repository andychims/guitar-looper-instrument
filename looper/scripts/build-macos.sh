#!/bin/bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
sdk_root="${BESPOKE_SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk}"
if [[ ! -d "$sdk_root" ]]; then
  echo "macOS 14.5 SDK missing at $sdk_root. Set BESPOKE_SDKROOT to an installed compatible SDK; no prerequisites are installed by this script." >&2
  exit 1
fi
if ! command -v cmake >/dev/null; then
  echo "CMake is missing. Install it separately, then rerun this script." >&2
  exit 1
fi
if [[ ! -f "$repo_root/libs/JUCE/CMakeLists.txt" ]]; then
  echo "JUCE submodule is missing. Initialize this checkout's submodules before building." >&2
  exit 1
fi
if ! /usr/bin/arch -arm64 /usr/bin/true 2>/dev/null; then
  echo "This build recipe needs an Apple Silicon Mac to run the native compiler while targeting x86_64." >&2
  exit 1
fi
export SDKROOT="$sdk_root"
python_arg=()
if [[ -n "${BESPOKE_PYTHON_ROOT:-}" ]]; then
  python_arg=("-DBESPOKE_PYTHON_ROOT=$BESPOKE_PYTHON_ROOT")
fi
cmake -S "$repo_root" -B "$repo_root/ignore/build-intel" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_SYSROOT="$SDKROOT" -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  '-DCMAKE_CXX_COMPILER_LAUNCHER=/usr/bin/arch;-arm64' \
  '-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/arch;-arm64' "${python_arg[@]}"
cmake --build "$repo_root/ignore/build-intel" --parallel 4 --config Release
