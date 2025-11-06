 
#!/bin/bash

set -euo pipefail

if [[ $# -gt 2 ]]; then
  echo "Usage: $0 [gcc|clang|msvc] [Debug|Release]" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPILER="${1:-gcc}"
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

BUILD_DIR="$SCRIPT_DIR/Bin/${COMPILER_NAME}-${BUILD_TYPE}/cmakeapp1"
BINARY_CANDIDATES=(
  "$BUILD_DIR/cmakeapp1"
  "$BUILD_DIR/${BUILD_TYPE}/cmakeapp1"
  "$BUILD_DIR/cmakeapp1.exe"
  "$BUILD_DIR/${BUILD_TYPE}/cmakeapp1.exe"
)

for binary in "${BINARY_CANDIDATES[@]}"; do
  if [[ -x "$binary" ]]; then
    exec "$binary"
  fi
done

echo "Unable to locate built executable for ${COMPILER_NAME}-${BUILD_TYPE}." >&2
echo "Ensure the project is configured and built before running." >&2
exit 1
