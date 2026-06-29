#!/usr/bin/env bash
# Run DSP unit tests and optional album integration checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MINI_FIXTURE="$ROOT/Tests/Fixtures/mini-album"
EVERMORE_DEFAULT="/Users/denispopkov/Downloads/Taylor Swift - evermore (Deluxe Edition) (2021) [24-88.2]"

cd "$ROOT"

if [[ ! -d "$MINI_FIXTURE" ]] || [[ -z "$(find "$MINI_FIXTURE" -maxdepth 1 -name '*.wav' -print -quit)" ]]; then
  echo "=== Generate mini album fixture ==="
  python3 "$ROOT/scripts/generate_test_fixture.py"
fi

if [[ -n "${CASSETTE_FIXTURE_ALBUM:-}" ]]; then
  ALBUM="$CASSETTE_FIXTURE_ALBUM"
elif [[ -d "$EVERMORE_DEFAULT" ]]; then
  ALBUM="$EVERMORE_DEFAULT"
else
  ALBUM="$MINI_FIXTURE"
fi

echo "=== Configure ==="
cmake -C cmake/ProdOptions.cmake -B build -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=ON

echo ""
echo "=== Build DeckTests ==="
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
cmake --build build --config Release --target DeckTests -j"$JOBS"

echo ""
echo "=== Unit + integration tests ==="
echo "Album fixture: $ALBUM"
echo "Set CASSETTE_SKIP_FULL_PREPARE=1 for a faster smoke run."
export CASSETTE_FIXTURE_ALBUM="$ALBUM"
ctest --test-dir build --output-on-failure

echo ""
echo "Audit complete."
