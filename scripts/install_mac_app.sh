#!/usr/bin/env bash
# Install / refresh Deck in Applications and rebuild icon cache.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_NAME="Deck"
BUILD_APP="$ROOT/build/Deck_artefacts/Release/${APP_NAME}.app"
DIST_APP="$ROOT/dist/macOS/${APP_NAME}.app"
DEST="/Applications/${APP_NAME}.app"
LEGACY_APPS=(
  "/Applications/CassetteMaster.app"
  "/Applications/Cassette Auto Master.app"
  "/Applications/CassetteAutoMaster.app"
)
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "=== Production configure (Pi tape OFF) ==="
cmake -C "$ROOT/cmake/ProdOptions.cmake" -B "$ROOT/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCASSETTE_BUILD_TESTS=OFF

echo "=== Build Deck ==="
cmake --build "$ROOT/build" --config Release --target Deck -j"$JOBS"

if [[ ! -d "$BUILD_APP" ]]; then
  echo "Build failed: $BUILD_APP not found"
  exit 1
fi

bash "$ROOT/scripts/verify_prod_app.sh" "$BUILD_APP"

bash "$ROOT/scripts/apply_app_icon.sh" "$BUILD_APP"

mkdir -p "$ROOT/dist/macOS"
rm -rf "$DIST_APP"
ditto "$BUILD_APP" "$DIST_APP"

echo "Installing to $DEST ..."
for legacy in "${LEGACY_APPS[@]}"; do
  if [[ -d "$legacy" && "$legacy" != "$DEST" ]]; then
    echo "Removing legacy app: $legacy"
    rm -rf "$legacy"
  fi
done
rm -rf "$DEST"
ditto "$BUILD_APP" "$DEST"
touch "$DEST"

bash "$ROOT/scripts/verify_prod_app.sh" "$DEST"

xattr -cr "$DEST" 2>/dev/null || true
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$DEST" 2>/dev/null || true
killall Dock 2>/dev/null || true

echo ""
echo "Готово:"
echo "  open -a \"$DEST\""
