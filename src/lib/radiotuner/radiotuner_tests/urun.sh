#!/bin/sh

#rm -rf build
current_dir=$(pwd)
executables=$(find . -path "*/build/*" -type f -perm +111 -mindepth 1 -maxdepth 3)
for executable in $executables; do
  rm -rf $executable
done

if which ninja >/dev/null; then
    cmake -B build -G Ninja && \
    ninja -C build $1
else
    cmake -B build && \
    make -j $(getconf _NPROCESSORS_ONLN) -C build $1
fi

