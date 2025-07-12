#!/bin/bash
export BUILD_DIR_NAME=${BUILD_DIR_NAME:-release_build_docker}
export PICO_PLATFORM=${PICO_PLATFORM:-rp2040}
export PICO_BOARD=${PICO_BOARD:-pico_w}

echo "Using ninja to build for $PICO_BOARD on $PICO_PLATFORM"

mkdir -p /opt/src/pico/"$BUILD_DIR_NAME"
cd /opt/src/pico || exit

cmake -B "$BUILD_DIR_NAME" -G Ninja \
  -DPICO_PLATFORM="$PICO_PLATFORM" \
  -DPICO_BOARD="$PICO_BOARD"

ninja -C "$BUILD_DIR_NAME" "$@"
