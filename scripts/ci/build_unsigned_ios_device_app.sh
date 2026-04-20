#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/ios-ci-device-app}"
DERIVED_DIR="${BUILD_DIR}/deriveddata"

cmake -S ios/ci -B "${BUILD_DIR}" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0

cmake --build "${BUILD_DIR}" --config Release -- \
  -derivedDataPath "${DERIVED_DIR}" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY=""

APP_PATH="$(find "${DERIVED_DIR}/Build/Products" -type d -name '*.app' | head -n1)"
if [[ -z "${APP_PATH}" ]]; then
  echo "Unsigned .app was not produced" >&2
  exit 1
fi

mkdir -p build/ios-app
rm -rf build/ios-app/AuroraIOS.app
cp -R "${APP_PATH}" build/ios-app/AuroraIOS.app
echo "Built unsigned app: build/ios-app/AuroraIOS.app"
