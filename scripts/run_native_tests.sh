#!/usr/bin/env bash
# PC-s unit tesztek PlatformIO nélkül (g++ + Unity). Használat: scripts/run_native_tests.sh [UNITY_SRC_DIR]
# Alapból a Unity forrást a PlatformIO-s `pio test -e native` hozza; ez a szkript gyors alternatíva.
set -euo pipefail
cd "$(dirname "$0")/.."
UNITY="${1:-/tmp/Unity/src}"
if [ ! -f "$UNITY/unity.c" ]; then
  git clone --depth 1 https://github.com/ThrowTheSwitch/Unity /tmp/Unity
  UNITY=/tmp/Unity/src
fi
python3 scripts/check_partitions.py
INC="-Ilib/Core -Ilib/Control -Ilib/TestSupport -I$UNITY"
rc=0
for t in selector interlock fsm misc; do
  echo "=== test_$t ==="
  g++ -std=gnu++17 -Wall -Wextra -DNATIVE_BUILD $INC test/test_$t/test_main.cpp lib/Control/*.cpp "$UNITY/unity.c" -o "/tmp/test_$t"
  "/tmp/test_$t" | grep -E "FAIL|Tests" || true
  "/tmp/test_$t" >/dev/null || rc=1
done
exit $rc
