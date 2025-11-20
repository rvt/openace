#!/bin/sh

if which ninja >/dev/null; then
    cmake -B release_build_2350 -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2_w -DGATAS_UART_OVER_USB=1 && \
    ninja -C release_build_2350  $1
else
    cmake -B release_build -DCMAKE_BUILD_TYPE=Release -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2_w && \
    make -j $(getconf _NPROCESSORS_ONLN) -C release_build_rp2350 $1 && \
    echo "done. P.S.: Consider installing ninja - it's faster"
fi

if test -d /volumes/RP2350; then
    echo "###############################"
    echo "## Copied to /volumes/RP2350  #"
    echo "###############################"
    cp release_build_2350/GATAS_rp2350-arm-s.uf2 /volumes/RP2350

else
    echo "########################"
    echo "## Please mount the PI #"
    echo "########################"
fi

exit $?