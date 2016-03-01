#!/bin/sh -xe

if [ "$CI_ARCH" = "amd64" ]; then
  export PATH=/mingw64/bin:$PATH
else
  export PATH=/mingw32/bin:$PATH
fi

7za | head -2
gcc -v
cppcheck --error-exitcode=1 src

export ELEVATE_VERSION=head
if [ -n "$CI_BUILD_TAG" ]; then
  export ELEVATE_VERSIOn=$CI_BUILD_TAG
fi
export ELEVATE_CFLAGS="-DELEVATE_VERSION=\\\"$ELEVATE_VERSION\\\""

make
file elevate.exe

if [ -n "$CI_BUILD_TAG" ]; then
  7za a elevate.7z elevate.exe
  echo $CI_BUILD_TAG > LATEST
fi

