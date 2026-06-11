#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PNG="${1:?Usage: make_icns.sh <ignored.png> <output.icns>}"
OUT_ICNS="${2:?Usage: make_icns.sh <ignored.png> <output.icns>}"
python3 "$ROOT/scripts/make_icns.py" "$OUT_ICNS"
