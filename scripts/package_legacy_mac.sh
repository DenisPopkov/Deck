#!/usr/bin/env bash
# Build and package Deck for Intel macOS 10.14 Mojave (x86_64).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
BUILD_DIR="$ROOT/build-legacy"
VERSION="$(sed -n 's/^project(Deck VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

cd "$ROOT"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Legacy macOS packaging must run on a Mac host (cross-compile to x86_64)." >&2
  exit 1
fi

if [[ ! -f Assets/AppIcon.png ]]; then
  echo "Missing Assets/AppIcon.png" >&2
  exit 1
fi

echo "=== macOS Legacy (Mojave 10.14, x86_64) ==="
cmake -C cmake/LegacyMacOptions.cmake -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCASSETTE_BUILD_TESTS=OFF \
  -DCASSETTE_SKIP_APP_ICON=ON
cmake --build "$BUILD_DIR" --config Release --target Deck -j"$JOBS"

APP_PATH="$BUILD_DIR/Deck_artefacts/Release/Deck.app"
if [[ ! -d "$APP_PATH" ]]; then
  echo "Build failed: $APP_PATH not found" >&2
  exit 1
fi

bash "$ROOT/scripts/verify_mac_app.sh" "$APP_PATH" --legacy

mkdir -p "$DIST/macOS-Legacy"
rm -rf "$DIST/macOS-Legacy/Deck.app"
cp -R "$APP_PATH" "$DIST/macOS-Legacy/"

bash "$ROOT/scripts/apply_app_icon.sh" "$DIST/macOS-Legacy/Deck.app"
bash "$ROOT/scripts/resign_mac_app.sh" "$DIST/macOS-Legacy/Deck.app"

ZIP="$DIST/Deck-${VERSION}-macOS-Mojave.zip"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$DIST/macOS-Legacy/Deck.app" "$ZIP"

echo ""
echo "OK — macOS Mojave release:"
echo "  App: $DIST/macOS-Legacy/Deck.app"
echo "  Zip: $ZIP"
echo ""
echo "Requires Intel Mac running macOS 10.14.6 or newer."
echo "First launch: right-click Deck.app → Open (unsigned app gatekeeper)."
