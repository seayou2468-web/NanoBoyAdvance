#!/usr/bin/env bash
set -euo pipefail

SDK="iphoneos"
TARGET="arm64-apple-ios26.0"
OUT_DIR="${1:-build/mikage-ios26-device}"
OBJ_DIR="${OUT_DIR}/obj"
LIB_PATH="${OUT_DIR}/libmikage_full.a"

mkdir -p "${OBJ_DIR}"

COMMON_INCLUDES=(
  -I.
  -Isrc
  -Isrc/core
  -Isrc/core/mikage
  -Isrc/core/mikage/reference/citra/src
  -Isrc/core/mikage/reference/citra/externals
  -Isrc/core/mikage/reference/citra/externals/glad/include
  -Isrc/core/mikage/reference/azahar/src
  -Isrc/core/mikage/reference/azahar/externals
)

COMMON_FLAGS=(
  -target "${TARGET}"
  -fPIC
  -DNBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION=1
  -D__APPLE__=1
  -DTARGET_OS_IPHONE=1
)

CPP_FLAGS=(
  -std=gnu++20
  "${COMMON_FLAGS[@]}"
  "${COMMON_INCLUDES[@]}"
)

C_FLAGS=(
  -std=gnu11
  "${COMMON_FLAGS[@]}"
  "${COMMON_INCLUDES[@]}"
)

mapfile -t SOURCES < <(find src/core/mikage -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.c' \) \
  ! -path 'src/core/mikage/reference/*' | sort)

if [[ ${#SOURCES[@]} -eq 0 ]]; then
  echo "No Mikage sources found" >&2
  exit 1
fi

rm -f "${LIB_PATH}"
objects=()

for src in "${SOURCES[@]}"; do
  rel="${src#src/core/mikage/}"
  obj="${OBJ_DIR}/${rel}.o"
  mkdir -p "$(dirname "${obj}")"

  if [[ "${src}" == *.c ]]; then
    xcrun --sdk "${SDK}" clang "${C_FLAGS[@]}" -c "${src}" -o "${obj}"
  else
    xcrun --sdk "${SDK}" clang++ "${CPP_FLAGS[@]}" -c "${src}" -o "${obj}"
  fi

  objects+=("${obj}")
done

xcrun --sdk "${SDK}" libtool -static -o "${LIB_PATH}" "${objects[@]}"

echo "Built ${LIB_PATH}"
