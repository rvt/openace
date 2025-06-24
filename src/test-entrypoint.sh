#!/bin/sh

cd /opt/src || exit
if which ninja >/dev/null; then
    cmake -B docker_build_test -G Ninja && \
    ninja -C docker_build_test $1
else
    cmake -B docker_build_test && \
    make -j $(getconf _NPROCESSORS_ONLN) -C docker_build_test $1 && \
    echo "done. P.S.: Consider installing ninja - it's faster"
fi

exit $?
