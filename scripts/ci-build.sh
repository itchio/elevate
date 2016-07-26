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
  export ELEVATE_VERSION=$CI_BUILD_TAG
fi
export CI_VERSION=$CI_BUILD_REF_NAME
export ELEVATE_CFLAGS="-DELEVATE_VERSION=\\\"$ELEVATE_VERSION\\\""

make
file elevate.exe

export CI_OS="windows"

# sign (win)
if [ "$CI_OS" = "windows" ]; then
  WIN_SIGN_KEY="Open Source Developer, Amos Wenger"
  WIN_SIGN_URL="http://timestamp.verisign.com/scripts/timstamp.dll"

  signtool.exe sign //v //s MY //n "$WIN_SIGN_KEY" //t "$WIN_SIGN_URL" elevate.exe
fi

# verify
7za a elevate.7z elevate.exe

# set up a file hierarchy that ibrew can consume, ie:
#
# - dl.itch.ovh
#   - elevate
#     - windows-amd64
#       - LATEST
#       - v0.3.0
#         - elevate.7z
#         - elevate.exe
#         - SHA1SUMS

BINARIES_DIR="binaries/$CI_OS-$CI_ARCH"
mkdir -p $BINARIES_DIR/$CI_VERSION
mv elevate.7z $BINARIES_DIR/$CI_VERSION
mv elevate.exe $BINARIES_DIR/$CI_VERSION

(cd $BINARIES_DIR/$CI_VERSION && sha1sum * > SHA1SUMS && sha256sum * > SHA256SUMS)

if [ -n "$CI_BUILD_TAG" ]; then
  echo $CI_BUILD_TAG > $BINARIES_DIR/LATEST
fi
