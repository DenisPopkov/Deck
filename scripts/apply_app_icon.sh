#!/usr/bin/env bash
# Regenerate squircle Icon.icns inside a .app bundle (overwrites JUCE square icon).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${1:?Usage: apply_app_icon.sh </path/to/App.app>}"

ICNS="$APP/Contents/Resources/Icon.icns"
if [[ ! -d "$APP" ]]; then
  echo "Not an app bundle: $APP" >&2
  exit 1
fi

python3 "$ROOT/scripts/round_app_icon.py"
python3 "$ROOT/scripts/make_icns.py" "$ICNS"

if [[ -f "$APP/Contents/Info.plist" ]]; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile Icon" "$APP/Contents/Info.plist" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Add :CFBundleIconName string Icon" "$APP/Contents/Info.plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Set :CFBundleIconName Icon" "$APP/Contents/Info.plist" 2>/dev/null || true
fi

touch "$ICNS"
touch "$APP"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP" 2>/dev/null || true
echo "Icon applied: $ICNS"
echo "Tip: if Dock still shows old shape, run: killall Dock"
