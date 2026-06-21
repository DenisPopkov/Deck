#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
VERSION="0.2.0"

cd "$ROOT"

if [[ ! -f Assets/AppIcon.png ]]; then
  echo "Missing Assets/AppIcon.png"
  exit 1
fi

echo "=== macOS Release ==="
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=ON
cmake --build build --config Release --target Deck DeckTests -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
ctest --test-dir build --output-on-failure

APP_NAME="Deck"
APP_PATH="build/Deck_artefacts/Release/${APP_NAME}.app"
if [[ ! -d "$APP_PATH" ]]; then
  APP_PATH="build/Deck_artefacts/Release/Deck.app"
  APP_NAME="Deck"
fi

mkdir -p "$DIST/macOS"
rm -rf "$DIST/macOS/${APP_NAME}.app"
cp -R "$APP_PATH" "$DIST/macOS/"

bash "$ROOT/scripts/apply_app_icon.sh" "$APP_PATH"
bash "$ROOT/scripts/resign_mac_app.sh" "$APP_PATH"

ZIP="$DIST/Deck-${VERSION}-macOS.zip"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$DIST/macOS/${APP_NAME}.app" "$ZIP"

echo ""
echo "OK — macOS release:"
echo "  App: $DIST/macOS/${APP_NAME}.app"
echo "  Zip: $ZIP"
echo ""
echo "Windows builds require Visual Studio 2022 — see README.md"
echo "First launch on macOS: right-click app → Open (unsigned app gatekeeper)"
