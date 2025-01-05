# Fetch ubuntu image
FROM ubuntu:24.10

# Install prerequisites
# pycryptodome is used for gatt compilation 
RUN \
    apt update && \
    apt install -y git python3 ninja-build cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib libusb-dev gnutls-bin python3-pycryptodome

# Install Pico SDK
RUN \
    cd /opt/ && \
    git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git --branch 2.1.0 && \
    cd pico-sdk/ && \
    git config fetch.recurseSubmodules on-demand && \
    git submodule update --init --depth 1 && \
    cd /

RUN \
    git config --global http.version HTTP/1.1 && \
    git config --global http.postBuffer 1048576000 && \
    git config --global https.postBuffer 1048576000 

RUN \
    cd /opt/ && \
    git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS.git --branch 202411.00 && \
    cd FreeRTOS && \
    git config fetch.recurseSubmodules on-demand && \
    git submodule update --init --depth 1 && \
    cd /


# Set Environments
ENV PICO_SDK_PATH=/opt/pico-sdk/
ENV FREERTOS_KERNEL_PATH=/opt/FreeRTOS/FreeRTOS/Source
ENV BUILD_DIR_NAME=release_build_docker

# Patch pico SDK and update btstack to dev branch
COPY ./pico-sdk.patch /opt/pico-sdk.patch

RUN ls -l /opt

RUN \
    cd ${PICO_SDK_PATH}lib/btstack && \
    git fetch --unshallow && \
    git checkout 5d4d8cc7b1d35a90bbd6d5ffd2d3050b2bfc861c && \
    cd ${PICO_SDK_PATH} && \
    git apply /opt/pico-sdk.patch

RUN \
    mkdir -p /opt/src/

# Create entrypoint script that will be used at container start
RUN echo '#!/bin/bash\n\
  echo "Using ninja"\n\
  mkdir -p /opt/src/pico/$BUILD_DIR_NAME\n\
  cd /opt/src/pico\n\
  cmake -B $BUILD_DIR_NAME -G Ninja && ninja -C $BUILD_DIR_NAME $1\n\
  exit' > /entrypoint.sh

RUN chmod +x /entrypoint.sh

# Use the custom entrypoint script to build and exit
ENTRYPOINT ["/entrypoint.sh"]
