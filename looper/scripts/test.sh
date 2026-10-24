#!/bin/bash
set -euo pipefail
looper_root="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$looper_root/scripts/patch_control/test_ai105.py"
test_binary="$(mktemp "${TMPDIR:-/tmp}/loop-grid-test.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror "$looper_root/tests/loop_grid_state_test.cpp" -o "$test_binary"
"$test_binary"
echo "LoopGrid state test passed"
c++ -std=c++17 -Wall -Wextra -Werror "$looper_root/tests/looper_commit_math_test.cpp" -o "$test_binary"
"$test_binary"
echo "Looper commit boundary test passed"
c++ -std=c++17 -Wall -Wextra -Werror "$looper_root/tests/test_performance_bindings.cpp" \
  "$looper_root/../Source/PerformanceBindings.cpp" -o "$test_binary"
"$test_binary"
echo "Performance binding test passed"
