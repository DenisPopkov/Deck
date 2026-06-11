#!/bin/bash
# GstPEAQ helper: prints ODG=-1.23 on success, exits non-zero if unavailable.
set -euo pipefail
REF="${1:?ref wav}"
TEST="${2:?test wav}"

if command -v peaqb >/dev/null 2>&1; then
  OUT=$(peaqb "$REF" "$TEST" 2>&1 || true)
  ODG=$(echo "$OUT" | grep -Eo 'ODG[^0-9-]*-?[0-9]+(\.[0-9]+)?' | head -1 | grep -Eo '-?[0-9]+(\.[0-9]+)?' || true)
  if [[ -n "$ODG" ]]; then
    echo "ODG=$ODG"
    exit 0
  fi
fi

if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
  echo "GstPEAQ: neither peaqb nor gst-launch-1.0 found" >&2
  exit 2
fi

if ! gst-inspect-1.0 peaq >/dev/null 2>&1; then
  echo "GstPEAQ: peaq element not installed (gstreamer plugins bad)" >&2
  exit 3
fi

# Gst peaq posts ODG on the bus; parse stderr/stdout for Diagnostic or use gst-launch -m
OUT=$(gst-launch-1.0 -m \
  filesrc location="$REF" ! decodebin ! audioconvert ! audio/x-raw,format=F32LE,channels=2 ! queue ! peaqa. \
  filesrc location="$TEST" ! decodebin ! audioconvert ! audio/x-raw,format=F32LE,channels=2 ! queue ! peaqa. \
  peaqa name=peaqa 2>&1 || true)

ODG=$(echo "$OUT" | grep -i odg | grep -Eo '-?[0-9]+\.[0-9]+|-?[0-9]+' | tail -1 || true)
if [[ -z "$ODG" ]]; then
  echo "GstPEAQ: could not parse ODG from pipeline output" >&2
  echo "$OUT" >&2
  exit 4
fi

echo "ODG=$ODG"
