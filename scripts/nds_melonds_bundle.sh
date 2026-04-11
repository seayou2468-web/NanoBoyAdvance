#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ARTIFACTS_DIR="${ROOT_DIR}/artifacts"
INPUT_DIR="${ARTIFACTS_DIR}/input"
FRAMES_DIR="${ARTIFACTS_DIR}/frames"
ZIP_PATH="${ARTIFACTS_DIR}/nds_melonds_bundle.zip"
PNG_ZIP_PATH="${ARTIFACTS_DIR}/nds_frames_only.zip"

ROM_NAME='Test-NTR - Actual Inspection Card NTR [AAAA01][NDS][World].nds'

sudo apt-get update
sudo apt-get install -y libsdl2-dev pkg-config p7zip-full zip curl unzip

mkdir -p "${INPUT_DIR}" "${FRAMES_DIR}"

curl -L --fail -o "${INPUT_DIR}/bios7.bin" 'https://archive.org/download/nds-bios-firmware/bios7.bin'
curl -L --fail -o "${INPUT_DIR}/bios9.bin" 'https://archive.org/download/nds-bios-firmware/bios9.bin'
curl -L --fail -o "${INPUT_DIR}/debug_cart.7z" 'https://tcrf.net/images/6/68/Nintendo_DS_Lite_USG_Debug_Cartridge.7z'
7z x -y -o"${INPUT_DIR}" "${INPUT_DIR}/debug_cart.7z"

make -C "${ROOT_DIR}" -j"$(nproc)"
make -C "${ROOT_DIR}" build/dump_frames

# Real screen launch (no dummy/offscreen driver): requires DISPLAY/WAYLAND_DISPLAY.
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
  echo "Launching real SDL frontend for melonDS core (close window or press ESC to continue)..."
  NDS_BIOS7_PATH="${INPUT_DIR}/bios7.bin" \
  NDS_BIOS9_PATH="${INPUT_DIR}/bios9.bin" \
    "${ROOT_DIR}/build/nanoboyadvance-linux" "${INPUT_DIR}/${ROM_NAME}"
else
  echo "No DISPLAY/WAYLAND_DISPLAY found. Skipping real-screen launch." >&2
fi

NDS_BIOS7_PATH="${INPUT_DIR}/bios7.bin" \
NDS_BIOS9_PATH="${INPUT_DIR}/bios9.bin" \
  "${ROOT_DIR}/build/dump_frames" "${INPUT_DIR}/${ROM_NAME}" "${FRAMES_DIR}" 120 30 30

(
  cd "${ARTIFACTS_DIR}"
  zip -r "${ZIP_PATH##*/}" \
    "input/bios7.bin" \
    "input/bios9.bin" \
    "input/${ROM_NAME}" \
    frames/*.png
)

(
  cd "${ARTIFACTS_DIR}"
  zip -r "${PNG_ZIP_PATH##*/}" frames/*.png
)

echo "Uploading full bundle zip..."
FULL_UPLOAD_JSON="$(curl -sS -F "file=@${ZIP_PATH}" https://tmpfiles.org/api/v1/upload)"
echo "${FULL_UPLOAD_JSON}"
echo
echo "Uploading PNG-only zip..."
PNG_UPLOAD_JSON="$(curl -sS -F "file=@${PNG_ZIP_PATH}" https://tmpfiles.org/api/v1/upload)"
echo "${PNG_UPLOAD_JSON}"
echo

{
  echo "full_bundle=${FULL_UPLOAD_JSON}"
  echo "png_only=${PNG_UPLOAD_JSON}"
} > "${ARTIFACTS_DIR}/share_urls.txt"
echo "Saved upload responses to ${ARTIFACTS_DIR}/share_urls.txt"
