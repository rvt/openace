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


executables=$(find . -path "*/build/*" -type f -perm +111 -mindepth 1 -maxdepth 3)

# Check if any executables were found
if [ -z "$executables" ]; then
  echo "No executables found in the build directory."
  exit 1
fi

# Iterate over each executable and execute them
for executable in $executables; do
  cd "$(dirname "${executable}")" && ./"$(basename $executable)"
  cd "${current_dir}"
  exit_code=$?
done

exit $exit_code
