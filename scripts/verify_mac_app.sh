#!/usr/bin/env bash
# Verify Deck.app architecture and minimum macOS version.
# Usage: verify_mac_app.sh <Deck.app> [--legacy|--modern]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <Deck.app> [--legacy|--modern]" >&2
  exit 1
fi

APP="$1"
MODE="${2:---modern}"
BIN="$APP/Contents/MacOS/Deck"

if [[ ! -x "$BIN" ]]; then
  echo "Missing executable: $BIN" >&2
  exit 1
fi

ARCHS="$(lipo -archs "$BIN" 2>/dev/null || true)"
MINOS=""
while IFS= read -r line; do
  if [[ "$line" =~ minos[[:space:]]+([0-9.]+) ]]; then
    MINOS="${BASH_REMATCH[1]}"
  fi
done < <(otool -l "$BIN" | grep -A3 LC_BUILD_VERSION || true)

if [[ -z "$MINOS" ]]; then
  while IFS= read -r line; do
    if [[ "$line" =~ version[[:space:]]+([0-9.]+) ]]; then
      MINOS="${BASH_REMATCH[1]}"
    fi
  done < <(otool -l "$BIN" | grep -A3 LC_VERSION_MIN_MACOSX || true)
fi

echo "Deck.app verification ($MODE)"
echo "  Architectures: ${ARCHS:-unknown}"
echo "  Minimum macOS: ${MINOS:-unknown}"

fail() {
  echo "FAIL: $1" >&2
  exit 1
}

version_le() {
  # Returns 0 when $1 <= $2 (lexicographic works for x.y.z).
  [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -n1)" == "$1" ]]
}

case "$MODE" in
  --legacy)
    [[ "$ARCHS" == *x86_64* ]] || fail "expected x86_64, got: $ARCHS"
    [[ -n "$MINOS" ]] || fail "could not read minimum macOS version"
    version_le "$MINOS" "10.14" || fail "expected min macOS <= 10.14, got $MINOS"
    ;;
  --modern)
    [[ "$ARCHS" == *arm64* ]] || fail "expected arm64, got: $ARCHS"
    [[ -n "$MINOS" ]] || fail "could not read minimum macOS version"
    version_le "11.0" "$MINOS" || fail "expected min macOS >= 11.0, got $MINOS"
    ;;
  *)
    fail "unknown mode: $MODE (use --legacy or --modern)"
    ;;
esac

echo "OK"
