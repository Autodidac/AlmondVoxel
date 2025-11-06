#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [gcc|clang|msvc] [Debug|Release]" >&2
  exit 1
fi

COMPILER="$1"
BUILD_TYPE="${2:-Debug}"

case "$COMPILER" in
  gcc)
    COMPILER_NAME="GCC"
    if command -v ninja >/dev/null 2>&1; then
      GENERATOR="Ninja"
    else
      GENERATOR="Unix Makefiles"
    fi
    COMPILER_ARGS=(-DCMAKE_CXX_COMPILER=g++)
    SINGLE_CONFIG=1
    ;;
  clang)
    COMPILER_NAME="Clang"
    if command -v ninja >/dev/null 2>&1; then
      GENERATOR="Ninja"
    else
      GENERATOR="Unix Makefiles"
    fi
    COMPILER_ARGS=(-DCMAKE_CXX_COMPILER=clang++)
    SINGLE_CONFIG=1
    ;;
  msvc)
    COMPILER_NAME="MSVC"
    GENERATOR="Ninja Multi-Config"
    COMPILER_ARGS=(-DCMAKE_CXX_COMPILER=cl.exe)
    SINGLE_CONFIG=0
    ;;
  *)
    echo "Usage: $0 [gcc|clang|msvc] [Debug|Release]" >&2
    exit 1
    ;;
 esac

BUILD_DIR="${PROJECT_ROOT}/Bin/${COMPILER_NAME}-${BUILD_TYPE}"
mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(-S "$PROJECT_ROOT" -B "$BUILD_DIR" -G "$GENERATOR")
CMAKE_ARGS+=("${COMPILER_ARGS[@]}")

if [[ $SINGLE_CONFIG -eq 1 ]]; then
  CMAKE_ARGS+=(-DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
else
  CMAKE_ARGS+=(-DCMAKE_CONFIGURATION_TYPES="${BUILD_TYPE}")
fi

if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
  CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi

if [[ -n "${VCPKG_INSTALLATION_ROOT:-}" && -f "${VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
  CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="${VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi

if [[ -z "${VCPKG_ROOT:-}" && -z "${VCPKG_INSTALLATION_ROOT:-}" ]]; then
  LOCAL_TOOLCHAIN="${PROJECT_ROOT}/vcpkg_installed/scripts/buildsystems/vcpkg.cmake"
  if [[ -f "$LOCAL_TOOLCHAIN" ]]; then
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$LOCAL_TOOLCHAIN")
  fi
fi

echo "Configuring ${COMPILER_NAME}-${BUILD_TYPE} in ${BUILD_DIR}" >&2
cmake "${CMAKE_ARGS[@]}"
