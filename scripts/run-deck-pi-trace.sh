#!/usr/bin/env bash
# Run Deck from terminal with Pi tape timing logs on stdout.
# Also written to ~/Desktop/Deck/session.log
set -euo pipefail

export DECK_PI_TRACE=1

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/build/Deck_artefacts/Release/Deck.app/Contents/MacOS/Deck"

if [[ ! -x "$APP" ]]; then
  APP="/Applications/Deck.app/Contents/MacOS/Deck"
fi

if [[ ! -x "$APP" ]]; then
  echo "Deck binary not found. Build first: cmake --build build --target Deck" >&2
  exit 1
fi

echo "Pi tape trace enabled (DECK_PI_TRACE=1)"
echo "Log file: ~/Desktop/Deck/session.log"
echo "Filter:   grep '\\[pi-tape\\]'"
echo "---"
exec "$APP" "$@"
