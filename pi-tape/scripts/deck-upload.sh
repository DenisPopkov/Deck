#!/usr/bin/env bash
# Upload a Deck-exported WAV + manifest to the Pi tape inbox.
# Usage: deck-upload.sh "/path/to/Album Side A.wav" [side_a|side_b|A|B]
set -euo pipefail

WAV="${1:-}"
SIDE_ARG="${2:-side_a}"

if [[ -z "$WAV" || ! -f "$WAV" ]]; then
  echo "Usage: $0 /path/to/exported.wav [side_a|side_b]" >&2
  exit 1
fi

FTP_URL="${PI_TAPE_FTP_URL:-ftp://tape:tape@127.0.0.1:2121/inbox}"
# Strip trailing slash from base URL
FTP_BASE="${FTP_URL%/}"

case "${SIDE_ARG,,}" in
  a|side_a|sidea) SIDE="A" ;;
  b|side_b|sideb) SIDE="B" ;;
  *) echo "Side must be side_a or side_b" >&2; exit 1 ;;
esac

BASENAME=$(basename "$WAV")
JOB_ID="$(date -u +%Y-%m-%dT%H:%M:%SZ)_$(echo "$BASENAME" | tr ' ' '_' | tr -cd '[:alnum:]._-')"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MANIFEST="$TMP/manifest.json"
cat > "$MANIFEST" <<EOF
{
  "job_id": "${JOB_ID}",
  "remote_path": "inbox/${BASENAME}",
  "side": "${SIDE}",
  "normalize": false,
  "target_format": "wav44k16stereo",
  "play_mode": "replace",
  "start_mode": "manual"
}
EOF

echo "Uploading ${BASENAME} (Side ${SIDE})..."
curl -fsS --ftp-create-dirs -T "$WAV" "${FTP_BASE}/${BASENAME}"

echo "Uploading manifest.json (after WAV is complete)..."
curl -fsS --ftp-create-dirs -T "$MANIFEST" "${FTP_BASE}/manifest.json"

echo "Job ${JOB_ID} ready. On Pi: sudo systemctl start pi-tape-play.service"
