# Fetch ubuntu image
FROM ubuntu:24.10

# Install prerequisites
RUN \
    apt update && \
    apt install -y git python3 ninja-build cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib libusb-dev gnutls-bin

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
    
# Used for gatt compilation
RUN \
    pip install pycryptodomex

RUN \
    mkdir -p /opt/src/

# Set Environments
ENV PICO_SDK_PATH=/opt/pico-sdk/
ENV FREERTOS_KERNEL_PATH=/opt/FreeRTOS/FreeRTOS/Source

#
# # Create entrypoint script that will be used at container start
RUN echo '#!/bin/bash\n\
  echo "Using ninja"\n\
  mkdir -p /opt/src/pico/release_build_docker\n\
  cd /opt/src/pico\n\
  cmake -B docker_build -G Ninja && ninja -C release_build_docker $1\n\
  exit' > /entrypoint.sh

RUN chmod +x /entrypoint.sh

# Use the custom entrypoint script to build and exit
ENTRYPOINT ["/entrypoint.sh"]
