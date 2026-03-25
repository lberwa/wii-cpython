# Makefile für reinen Wii-Build
SHELL := /bin/sh
ROOT := $(shell CDPATH= cd -- "$(dir $(realpath $(firstword $(MAKEFILE_LIST))))" && pwd)

# Toolchain
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
AR := $(DEVKITPPC)/bin/powerpc-eabi-ar
RANLIB := $(DEVKITPPC)/bin/powerpc-eabi-ranlib
LD := $(DEVKITPPC)/bin/powerpc-eabi-ld
NM := $(DEVKITPPC)/bin/powerpc-eabi-nm
STRIP := $(DEVKITPPC)/bin/powerpc-eabi-strip
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy
OBJDUMP := $(DEVKITPPC)/bin/powerpc-eabi-objdump
READELF := $(DEVKITPPC)/bin/powerpc-eabi-readelf

OPT := -g -O2 -Wall -Wstrict-prototypes -fPIC
CFLAGS_WII := -Os -Wall -msoft-float

.PHONY: all clean wii python-wii cpython zlib curl

all: wii

# ----------------------------
# Clean
# ----------------------------
clean:
	rm -rf $(ROOT)/build-wii
	rm -rf $(ROOT)/curl/build-wii $(ROOT)/curl/mbedtls/build-wii $(ROOT)/curl/mbedtls/install-wii
	cd $(ROOT)/zlib && make distclean || true

# ----------------------------
# Wii Build
# ----------------------------
wii: zlib python-wii curl cpython

# ----------------------------
# zlib für Wii
# ----------------------------
zlib:
	cd $(ROOT)/zlib && \
	export DEVKITPRO=$(DEVKITPRO) && export DEVKITPPC=$(DEVKITPPC) && export PATH=$(DEVKITPPC)/bin:$$PATH && \
	export CC=$(CC) && export AR=$(AR) && export RANLIB=$(RANLIB) && export CFLAGS="$(CFLAGS_WII)" && \
	make distclean || true && \
	./configure --static --prefix=$$(pwd)/install-wii && \
	make libz.a V=1

# ----------------------------
# CPython konfigurieren für Wii
# ----------------------------
python-wii:
	mkdir -p $(ROOT)/build-wii
	cd $(ROOT)/build-wii && \
	export DEVKITPRO=$(DEVKITPRO) && export DEVKITPPC=$(DEVKITPPC) && \
	export CC=$(CC) && export CXX=$(CXX) && export AR=$(AR) && export RANLIB=$(RANLIB) && \
	export LD=$(LD) && export NM=$(NM) && export STRIP=$(STRIP) && export OBJCOPY=$(OBJCOPY) && export OBJDUMP=$(OBJDUMP) && export READELF=$(READELF) && \
	../configure --host=powerpc-eabi --build=$$(../config.guess) \
		--without-ensurepip --disable-shared --disable-ipv6 --with-mimalloc=no

# ----------------------------
# mbedTLS + curl für Wii
# ----------------------------
curl:
	$(MAKE) -C $(ROOT)/build-wii -j8 curl-wii OPT="$(OPT)"

# ----------------------------
# CPython bauen (libpython.a)
# ----------------------------
cpython:
	$(MAKE) -C $(ROOT)/build-wii -j8 libpython.a