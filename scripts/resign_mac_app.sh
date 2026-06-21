#!/usr/bin/env bash
# Re-seal the .app after post-build resource changes (icon, scripts, models).
set -euo pipefail

APP="${1:?Usage: resign_mac_app.sh </path/to/App.app>}"

if [[ ! -d "$APP" ]]; then
  echo "Not an app bundle: $APP" >&2
  exit 1
fi

codesign --force --deep --sign - "$APP"
codesign --verify --verbose "$APP"
