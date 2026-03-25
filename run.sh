#! /bin/sh
set -e

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ $# -gt 0 ]; then
    echo "no clean"
else
    rm -rf "$ROOT/build-host" "$ROOT/build-wii"
    rm -rf "$ROOT/curl/build-wii" "$ROOT/curl/mbedtls/build-wii" "$ROOT/curl/mbedtls/install-wii"
fi

# Ensure debug symbols across all builds
OPT="-g -O2 -Wall -Wstrict-prototypes -fPIC"

# Build host Python (needed for cross build)
if [ ! -x "$ROOT/build-host/python" ]; then
  mkdir -p "$ROOT/build-host"
  cd "$ROOT/build-host"
  ../configure --without-ensurepip
  make -j8
fi

# Configure Wii cross build
if [ ! -f "$ROOT/build-wii/pyconfig.h" ]; then
  cd "$ROOT"
  mkdir -p "$ROOT/build-wii"
  cd "$ROOT/build-wii"
  export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
  export DEVKITPPC=${DEVKITPPC:-$DEVKITPRO/devkitPPC}
  export CC="$DEVKITPPC/bin/powerpc-eabi-gcc"
  export CXX="$DEVKITPPC/bin/powerpc-eabi-g++"
  export AR="$DEVKITPPC/bin/powerpc-eabi-ar"
  export RANLIB="$DEVKITPPC/bin/powerpc-eabi-ranlib"
  export LD="$DEVKITPPC/bin/powerpc-eabi-ld"
  export NM="$DEVKITPPC/bin/powerpc-eabi-nm"
  export STRIP="$DEVKITPPC/bin/powerpc-eabi-strip"
  export OBJCOPY="$DEVKITPPC/bin/powerpc-eabi-objcopy"
  export OBJDUMP="$DEVKITPPC/bin/powerpc-eabi-objdump"
  export READELF="$DEVKITPPC/bin/powerpc-eabi-readelf"
  export ac_cv_file__dev_ptmx=no
  export ac_cv_file__dev_ptc=no
  export ac_cv_header_sys_resource_h=no
  export ac_cv_func_getrlimit=no
  export ac_cv_func_setrlimit=no
  export ac_cv_header_sys_statvfs_h=no
  export ac_cv_func_statvfs=no
  ../configure --host=powerpc-eabi --build=$(../config.guess) \
    --with-build-python=../build-host/python \
    --without-ensurepip --disable-shared --disable-ipv6 --with-mimalloc=no
fi

# Build zlib for Wii
cd $ROOT/zlib/
echo "ZLIB"
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH
export CC=powerpc-eabi-gcc
export AR=powerpc-eabi-ar
export RANLIB=powerpc-eabi-ranlib
export CFLAGS="-Os -Wall -msoft-float"

make distclean  # falls configure.log da
./configure --static --prefix=$(pwd)/install-wii
make libz.a V=1
echo "ZLIB done"
cd $ROOT

# Build mbedTLS + curl for Wii (use build-wii Makefile)
make -C "$ROOT/build-wii" -j8 curl-wii OPT="$OPT"

# Build CPython for Wii (use build-wii Makefile)
make -C "$ROOT/build-wii" -j8 OPT="$OPT"
