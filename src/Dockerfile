FROM ubuntu:24.10

ENV DEBIAN_FRONTEND=noninteractive
ENV PICO_SDK_PATH=/opt/pico-sdk
ENV FREERTOS_KERNEL_PATH=/opt/FreeRTOS-Kernel

# Install everything needed
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        git python3 ninja-build cmake gcc-arm-none-eabi \
        libnewlib-arm-none-eabi build-essential \
        libstdc++-arm-none-eabi-newlib libusb-dev gnutls-bin \
        python3-pycryptodome && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /opt

# Git config and clone dependencies
RUN git config --global http.version HTTP/1.1 && \
    git config --global http.postBuffer 1048576000 && \
    git config --global https.postBuffer 1048576000 && \
    git clone --branch 2.1.1 --depth 1 https://github.com/raspberrypi/pico-sdk.git && \
    cd pico-sdk && \
    git config fetch.recurseSubmodules on-demand && \
    git submodule update --init --depth 1 && \
    cd lib/btstack && \
    git fetch --unshallow && git checkout v1.6.2 && \
    cd /opt && \
    git clone --branch V11.2.0 --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git && \
    cd FreeRTOS-Kernel && \
    git config fetch.recurseSubmodules on-demand && \
    git submodule update --init --depth 1

COPY ./freertos.patch /opt/
COPY ./Community-Supported-Ports.patch /opt/
RUN cd /opt/FreeRTOS-Kernel && git apply /opt/freertos.patch && cd /opt/FreeRTOS-Kernel/portable/ThirdParty/Community-Supported-Ports && git apply /opt/Community-Supported-Ports.patch

RUN mkdir -p /opt/src/

# Copy the entrypoints 
COPY ./build-entrypoint.sh /build-entrypoint.sh
COPY ./test-entrypoint.sh /test-entrypoint.sh

RUN chmod +x /build-entrypoint.sh /test-entrypoint.sh
