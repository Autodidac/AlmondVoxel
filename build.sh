#!/bin/bash
# Usage: ./build.sh [gcc|clang|msvc] [Debug|Release]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
CONFIGURE_SCRIPT="$PROJECT_ROOT/cmake/configure.sh"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [gcc|clang|msvc] [Debug|Release]" >&2
  exit 1
fi

COMPILER="$1"
BUILD_TYPE="${2:-Debug}"

case "$COMPILER" in
  gcc)
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_NAME="Clang"
    ;;
  msvc)
    COMPILER_NAME="MSVC"
    ;;
  *)
    echo "Usage: $0 [gcc|clang|msvc] [Debug|Release]" >&2
    exit 1
    ;;
esac

BUILD_DIR="$PROJECT_ROOT/Bin/${COMPILER_NAME}-${BUILD_TYPE}"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  "$CONFIGURE_SCRIPT" "$COMPILER" "$BUILD_TYPE"
fi

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --verbose
