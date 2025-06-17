#!/bin/sh

if which ninja >/dev/null; then
    cmake -B release_build_2040 -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_PLATFORM=rp2040 -DPICO_BOARD=pico_w && \
    ninja -C release_build_2040  $1
else
    cmake -B release_build_2040 -DCMAKE_BUILD_TYPE=Release -DPICO_PLATFORM=rp2040 -DPICO_BOARD=pico_w && \
    make -j $(getconf _NPROCESSORS_ONLN) -C release_build_2040 $1 && \
    echo "done. P.S.: Consider installing ninja - it's faster"
fi

if test -d /volumes/RPI-RP2; then
    echo "###############################"
    echo "## Copied to /volumes/RPI-RP2 #"
    echo "###############################"
    cp release_build/GaTas.uf2 /volumes/RPI-RP2

else
    echo "########################"
    echo "## Please mount the PI #"
    echo "########################"
fi

exit $?