#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OUT_LIB="$BUILD_DIR/gui/libyuan_gui_linux.so"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "yuan_gui_linux only supports Linux." >&2
  exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target yuan_gui_linux

echo "Built: $OUT_LIB"
