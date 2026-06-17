#!/usr/bin/env bash
# Capture Deck.app window screenshots for the GitHub Pages site.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/build/Deck_artefacts/Release/Deck.app"
OUT="$ROOT/docs/assets"

if [[ ! -d "$APP" ]]; then
  echo "Build Deck first: cmake --build build --target Deck"
  exit 1
fi

mkdir -p "$OUT"

# Close any running instance so we get a clean window.
osascript -e 'tell application "Deck" to quit' 2>/dev/null || true
sleep 1

open -a "$APP"
sleep 3

capture_window() {
  local outfile="$1"
  local wid
  wid=$(osascript <<'APPLESCRIPT'
tell application "System Events"
  tell process "Deck"
    if (count of windows) is 0 then error "Deck window not found"
    return id of front window
  end tell
end tell
APPLESCRIPT
  )
  screencapture -x -l"$wid" "$outfile"
  echo "Wrote $outfile"
}

capture_window "$OUT/screenshot-hero.png"

# Resize for web (max width 1200px).
for img in "$OUT"/screenshot-*.png; do
  [[ -f "$img" ]] || continue
  sips -Z 1200 "$img" >/dev/null
done

osascript -e 'tell application "Deck" to quit' 2>/dev/null || true

echo "Done."
