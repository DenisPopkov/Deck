#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
VERSION="${3:-0.0.0}"

cd "$ROOT"

EXE="$("$ROOT/scripts/find_deck_binary.sh" "$BUILD_DIR" "$CONFIG")"
DIST="$ROOT/dist/Linux"
STAGE="$DIST/Deck-$VERSION"
TARBALL="$ROOT/dist/Deck-$VERSION-Linux.tar.gz"

rm -rf "$STAGE"
mkdir -p "$STAGE/scripts" "$STAGE/models"

cp "$EXE" "$STAGE/Deck"
chmod +x "$STAGE/Deck"
cp "$ROOT/scripts/run_peaq.sh" "$STAGE/scripts/"

if [[ -f "$ROOT/models/tape_stn.onnx" ]]; then
  cp "$ROOT/models/tape_stn.onnx" "$STAGE/models/"
fi

rm -f "$TARBALL"
tar -czf "$TARBALL" -C "$DIST" "Deck-$VERSION"

echo ""
echo "Package ready:"
echo "  Folder: $STAGE"
echo "  Tarball: $TARBALL"
echo "Run: $STAGE/Deck"
