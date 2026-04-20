#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${ROOT_DIR}"

# "evaluate" などの一般語に誤爆しないよう、Lua C API/ヘッダ名に限定して検査する
LUA_API_PATTERN='lua_State|luaL_|luaopen_|lauxlib\.h|lualib\.h|#include <lua|#include "lua'
LUA_BUNDLED_PATH_PATTERN='src/core/mikage/reference/Panda3DS/include/lua'

if rg -n "${LUA_API_PATTERN}" src/core/mikage src/core/api src/platform 2>/dev/null; then
  echo "error: Lua API reference detected. Lua runtime support has been removed." >&2
  exit 1
fi

if rg -n "${LUA_BUNDLED_PATH_PATTERN}" src/core scripts Config \
  -g '!scripts/ci/check_no_lua_dependency.sh' \
  -g '!PANDA3DS_EXTERNAL_DEPENDENCIES.md' 2>/dev/null; then
  echo "error: Bundled Lua path reference detected. Lua must remain fully removed." >&2
  exit 1
fi

echo "ok: no Lua dependency references found"
