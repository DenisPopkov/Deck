#!/usr/bin/env bash
# Fail if a production app bundle still contains Pi tape UI strings.
set -euo pipefail

APP="${1:?Usage: verify_prod_app.sh </path/to/Deck.app>}"
BIN="$APP/Contents/MacOS/Deck"

if [[ ! -x "$BIN" ]]; then
  echo "Deck binary not found: $BIN" >&2
  exit 1
fi

if strings "$BIN" | grep -qi "Raspberry Pi tape deck"; then
  echo "ERROR: Pi tape UI found in production build: $APP" >&2
  echo "Reconfigure with -DCASSETTE_ENABLE_PI_TAPE=OFF (see cmake/ProdOptions.cmake)." >&2
  exit 1
fi

if strings "$BIN" | grep -qi "Send to Pi"; then
  echo "ERROR: Pi tape control found in production build: $APP" >&2
  exit 1
fi

echo "OK: production app has no Pi tape UI ($APP)"
