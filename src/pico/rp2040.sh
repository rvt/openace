#!/bin/sh

set -e

### ---- Build type selection ----
BUILD_TYPE="${1:-Release}"

case "$BUILD_TYPE" in
    Debug|Release)
        if [ $# -gt 0 ]; then
            shift
        fi
        ;;
    *)
        echo "Invalid build type: $BUILD_TYPE"
        echo "Use: Debug or Release"
        exit 1
        ;;
esac



BUILD_DIR="${BUILD_TYPE}_build_2040"
UF2_NAME="GATAS_rp2040.uf2"
TARGET_DIR="/volumes/RPI-RP2"

CMAKE_ARGS="
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
  -DPICO_PLATFORM=rp2040
  -DPICO_BOARD=pico_w
  -DGATAS_UART_OVER_USB=1
"

echo "Configuring build directory: ${BUILD_DIR}"

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="-G Ninja"
    BUILD_CMD="ninja -C ${BUILD_DIR} $1"
else
    GENERATOR=""
    BUILD_CMD="make -j $(getconf _NPROCESSORS_ONLN) -C ${BUILD_DIR} $1"
    echo "Note: Ninja not found — using make (slower)."
fi

cmake -B "${BUILD_DIR}" ${GENERATOR} ${CMAKE_ARGS}
eval "${BUILD_CMD}"

# Copying stage
UF2_PATH="${BUILD_DIR}/${UF2_NAME}"

if [ -d "${TARGET_DIR}" ]; then
    echo "###############################"
    echo "## Copying to ${TARGET_DIR}"
    echo "###############################"
    cp "${UF2_PATH}" "${TARGET_DIR}"
else
    echo "########################"
    echo "## Please mount the PI #"
    echo "########################"
fi
