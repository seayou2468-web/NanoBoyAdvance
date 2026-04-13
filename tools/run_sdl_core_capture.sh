#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OUT_DIR="${1:-artifacts/frames}"
ZIP_PATH="${2:-artifacts/frames.zip}"
ROM_PATH="${3:-src/core/quick_nes/dq.nes}"

mkdir -p "$(dirname "$ZIP_PATH")" "$OUT_DIR"

echo "[1/4] Building dump-frames tool"
make -j"$(nproc)" dump-frames

echo "[2/4] Capturing frames"
./build/dump_frames "$ROM_PATH" "$OUT_DIR" 5 1 1

echo "[3/4] Zipping PNG frames"
rm -f "$ZIP_PATH"
zip -j "$ZIP_PATH" "$OUT_DIR"/*.png >/dev/null

echo "[4/4] Uploading with curl"
URL="$(curl --fail --silent --show-error --upload-file "$ZIP_PATH" https://transfer.sh/$(basename "$ZIP_PATH"))"
echo "Uploaded: $URL"
