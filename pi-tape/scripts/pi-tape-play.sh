#!/usr/bin/env bash
# Fetch tape job manifest + WAV, convert for deck, play via ALSA.
set -euo pipefail

CONFIG="${PI_TAPE_CONFIG:-/etc/pi-tape/config.env}"
DRY_RUN=0

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    --config=*) CONFIG="${arg#*=}" ;;
  esac
done

if [[ ! -f "$CONFIG" ]]; then
  echo "Missing config: $CONFIG" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "$CONFIG"

PI_TAPE_ROOT="${PI_TAPE_ROOT:-/srv/pi-tape}"
LOCAL_INBOX="${LOCAL_INBOX:-$PI_TAPE_ROOT/inbox}"
WORK="$PI_TAPE_ROOT/work"
ARCHIVE="$PI_TAPE_ROOT/archive"
FAILED="$PI_TAPE_ROOT/failed"
STATE_FILE="${STATE_FILE:-/var/lib/pi-tape/last_job_id}"
LOCK_FILE="/run/pi-tape-play.lock"
TRANSPORT="${TRANSPORT:-ftp}"

mkdir -p "$WORK" "$ARCHIVE" "$FAILED" "$(dirname "$STATE_FILE")"

exec 9>"$LOCK_FILE"
if ! flock -n 9; then
  echo "Another pi-tape-play run is active"
  exit 0
fi

log() { echo "[pi-tape] $*"; }

fetch_file() {
  local remote="$1" local_path="$2"
  case "$TRANSPORT" in
    ftp)
      curl -fsS --ftp-pasv \
        -u "${FTP_USER}:${FTP_PASS}" \
        "ftp://${FTP_HOST}:${FTP_PORT}/${remote}" \
        -o "${local_path}.part"
      ;;
    sftp)
      local identity=()
      [[ -n "${SFTP_IDENTITY:-}" ]] && identity=(-i "$SFTP_IDENTITY")
      sftp "${identity[@]}" -b - "${SFTP_USER}@${SFTP_HOST}" <<EOF
get ${remote} ${local_path}.part
EOF
      ;;
    local)
      local rel
      rel=$(resolve_remote_path "$remote")
      cp "${PI_TAPE_ROOT}/${rel}" "${local_path}.part"
      ;;
    *)
      echo "Unknown TRANSPORT=$TRANSPORT" >&2
      return 1
      ;;
  esac
  mv "${local_path}.part" "$local_path"
}

manifest_remote() {
  case "$TRANSPORT" in
    ftp) echo "${FTP_REMOTE_DIR}/manifest.json" ;;
    sftp) echo "${SFTP_REMOTE_DIR}/manifest.json" ;;
    local) echo "manifest.json" ;;
  esac
}

resolve_remote_path() {
  local path="$1"
  if [[ "$path" == /* ]]; then
    echo "${path#/}"
  else
    echo "$path"
  fi
}

read_last_job() {
  [[ -f "$STATE_FILE" ]] && cat "$STATE_FILE" || true
}

write_last_job() {
  echo "$1" > "$STATE_FILE"
}

MANIFEST="$WORK/manifest.json"
rm -f "$MANIFEST"

log "Fetching manifest ($(manifest_remote))"
fetch_file "$(manifest_remote)" "$MANIFEST"

JOB_ID=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['job_id'])" "$MANIFEST")
REMOTE_PATH=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['remote_path'])" "$MANIFEST")
NORMALIZE=$(python3 -c "import json,sys; print('1' if json.load(open(sys.argv[1])).get('normalize') else '0')" "$MANIFEST")
SIDE=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1])).get('side','?'))" "$MANIFEST")

LAST=$(read_last_job)
if [[ "$LAST" == "$JOB_ID" ]]; then
  log "Job $JOB_ID already played; skipping"
  exit 0
fi

REMOTE=$(resolve_remote_path "$REMOTE_PATH")
RAW="$WORK/source.wav"
READY="$WORK/ready.wav"

log "Downloading Side $SIDE: $REMOTE"
fetch_file "$REMOTE" "$RAW"

if [[ "$NORMALIZE" == "1" ]]; then
  log "Converting + loudnorm"
  ffmpeg -hide_banner -loglevel error -y \
    -i "$RAW" \
    -af "loudnorm=I=${LOUDNORM_I:-18}:LRA=7:TP=${LOUDNORM_TP:--3},aresample=44100" \
    -ac 2 -c:a pcm_s16le "$READY"
else
  log "Converting to 44.1kHz 16-bit stereo"
  ffmpeg -hide_banner -loglevel error -y \
    -i "$RAW" \
    -ac 2 -ar 44100 -c:a pcm_s16le "$READY"
fi

if [[ "$DRY_RUN" == "1" ]]; then
  DUR=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$READY" 2>/dev/null || echo "?")
  log "Dry run — prepared $READY (${DUR}s)"
  exit 0
fi

APLAY_ARGS=()
[[ -n "${ALSA_DEVICE:-}" ]] && APLAY_ARGS=(-D "$ALSA_DEVICE")

log "Playing Side $SIDE on ${ALSA_DEVICE:-default}"
START=$(date -Iseconds)
aplay "${APLAY_ARGS[@]}" "$READY"
END=$(date -Iseconds)

STAMP=$(date +%Y%m%dT%H%M%S)
ARCHIVE_BASE="$ARCHIVE/${STAMP}_${JOB_ID}"
mkdir -p "$ARCHIVE_BASE"
cp "$MANIFEST" "$RAW" "$READY" "$ARCHIVE_BASE/"

write_last_job "$JOB_ID"
log "Done Side $SIDE ($START → $END), archived to $ARCHIVE_BASE"
