#!/bin/bash
# Copy tape_stn.onnx into app/plugin Resources when the model file exists.
set -euo pipefail
SRC="${1:?source onnx path}"
DST="${2:?destination file path}"
if [ -f "$SRC" ]; then
  mkdir -p "$(dirname "$DST")"
  cp "$SRC" "$DST"
  echo "Bundled STN ONNX: $DST"
fi
