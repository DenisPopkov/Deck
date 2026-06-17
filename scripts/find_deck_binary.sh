#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
RUN=false

if [[ "${3:-}" == "--run" ]]; then
  RUN=true
fi

candidates=(
  "$BUILD_DIR/Deck_artefacts/$CONFIG/Deck"
  "$BUILD_DIR/Deck_artefacts/Deck"
)

for path in "${candidates[@]}"; do
  if [[ -x "$path" ]]; then
    if $RUN; then
      exec "$path"
    fi
    echo "$path"
    exit 0
  fi
done

found="$(find "$BUILD_DIR/Deck_artefacts" -maxdepth 3 -type f -name Deck -perm -111 2>/dev/null | head -n 1 || true)"
if [[ -n "$found" ]]; then
  if $RUN; then
    exec "$found"
  fi
  echo "$found"
  exit 0
fi

echo "Deck binary not found under $BUILD_DIR/Deck_artefacts" >&2
exit 1
