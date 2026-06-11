#!/usr/bin/env bash
# Install / refresh Cassette Auto Master in Applications and rebuild icon cache.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_NAME="CassetteMaster"
BUILD_APP="$ROOT/build/CassetteAutoMaster_artefacts/Release/${APP_NAME}.app"
DIST_APP="$ROOT/dist/macOS/${APP_NAME}.app"
DEST="/Applications/${APP_NAME}.app"

if [[ ! -d "$BUILD_APP" ]]; then
  echo "Build first: cmake --build build --config Release --target CassetteAutoMaster"
  exit 1
fi

bash "$ROOT/scripts/apply_app_icon.sh" "$BUILD_APP"

mkdir -p "$ROOT/dist/macOS"
rm -rf "$DIST_APP"
ditto "$BUILD_APP" "$DIST_APP"

echo "Installing to $DEST ..."
rm -rf "$DEST"
ditto "$BUILD_APP" "$DEST"
touch "$DEST"

xattr -cr "$DEST" 2>/dev/null || true
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$DEST" 2>/dev/null || true
killall Dock 2>/dev/null || true

echo ""
echo "Готово:"
echo "  open -a \"$DEST\""
