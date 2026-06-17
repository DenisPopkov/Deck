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

if [[ ! -d "$BUILD_APP" ]]; then
  echo "Build first: cmake --build build --config Release --target Deck"
  exit 1
fi

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

xattr -cr "$DEST" 2>/dev/null || true
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$DEST" 2>/dev/null || true
killall Dock 2>/dev/null || true

echo ""
echo "Готово:"
echo "  open -a \"$DEST\""
