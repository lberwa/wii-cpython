# Wii-only build (no host build). Uses Python 3.15 configure output to build libpython.a.

SHELL := /bin/sh
VERSION=		3.15
srcdir=			.
DEVKITPRO ?= $(DEVKITPRO)
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
BUILD_DIR ?= build-wii
LIB_DIR ?= libs
BUILD_HOST_DIR ?= build-host
BUILD_DIR_ABS := $(abspath $(BUILD_DIR))
LIB_DIR_ABS := $(abspath $(LIB_DIR))
BUILD_HOST_DIR_ABS := $(abspath $(BUILD_HOST_DIR))
HOST_BUILD_PYTHON := $(abspath $(BUILD_HOST_DIR))/python
SYSTEM_PYTHON3 := $(shell command -v python3 2>/dev/null)
BUILD_PYTHON ?= $(HOST_BUILD_PYTHON)
CONFIG_SITE_FILE := $(abspath config.site)
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CPU_CORES := $(shell nproc)
TMPDIR=$(MAKEFILE_DIR)tmp

# Wii Cross-Tools (from current Makefile)
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

OPT=			-mhard-float -g -O2 -Wall -Wstrict-prototypes -fPIC
CFLAGS=			$(OPT)
WII_DEFINES := \
	-DWII_BUILD \
	-D__WII__ \
	-D__wii__ \
	-D__PPC__ \
	-D__powerpc__ \
	-DWII_SINGLE_THREAD=1 \
	-DTERMINAL_PRINT_DEBUG
WII_INCLUDE_DIRS := \
	-I. \
	-I$(MAKEFILE_DIR)bitmap/include \
	-I$(MAKEFILE_DIR)fat/include \
	-I$(DEVKITPRO)/libogc/include \
	-I$(DEVKITPRO)/libogc/gc \
	-I$(DEVKITPRO)/libogc/gc/ogc
CFLAGS_WII := -Os -Wall $(WII_DEFINES) $(WII_INCLUDE_DIRS)
CPPFLAGS_WII := $(WII_DEFINES) $(WII_INCLUDE_DIRS)
MACHDEP=		wii
prefix=			/usr/local
exec_prefix=		${prefix}
BINDIR=			$(exec_prefix)/bin
LIBDIR=			$(exec_prefix)/lib
SCRIPTDIR=		$(prefix)/lib
LIBRARY=		libpython$(VERSION).a
LDLIBRARY=		libpython$(VERSION).a
SO=				.so
LDSHARED=		$(CC) -shared
CCSHARED=		-fpic
LINKFORSHARED=		-Xlinker -export-dynamic
SHELL=			/bin/sh
LN=				ln
INSTALL=		./install-sh -c
DIRMODE=		755
EXEMODE=		755
FILEMODE=		644

CONFIGURE_ENV= \
	DEVKITPRO="$(DEVKITPRO)" \
	DEVKITPPC="$(DEVKITPPC)" \
	CONFIG_SITE="$(CONFIG_SITE_FILE)" \
	CC="$(CC)" CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" \
	LD="$(LD)" NM="$(NM)" STRIP="$(STRIP)" OBJCOPY="$(OBJCOPY)" \
	OBJDUMP="$(OBJDUMP)" READELF="$(READELF)" \
	CFLAGS="$(CFLAGS) $(CFLAGS_WII)" \
	CPPFLAGS="$(CPPFLAGS) $(CPPFLAGS_WII)" \
	ax_cv_c_float_words_bigendian=yes \
	ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no \
	ac_cv_header_sys_resource_h=no ac_cv_func_getrlimit=no ac_cv_func_setrlimit=no \
	ac_cv_header_sys_statvfs_h=no ac_cv_func_statvfs=no

CONFIGURE_FLAGS= \
	--host=powerpc-eabi --build=$$(../config.guess) \
	--with-build-python="$(BUILD_PYTHON)" \
	--without-ensurepip --disable-shared --disable-ipv6 --with-mimalloc=no

.PHONY: all clean configure libpython build-host python curl ssl wiitest	\
 		regen-importlib bitmap bitmap-clean fat fat-clean wiitest-clean py

all: wiitest

bitmap:
	$(MAKE) -j$(CPU_CORES) -C $(MAKEFILE_DIR)bitmap

bitmap-clean:
	@$(MAKE) -C $(MAKEFILE_DIR)bitmap clean

cp-libs: python curl bitmap fat
	@rm -rf "$(LIB_DIR)"
	@mkdir -p "$(LIB_DIR)"
	@find "$(MAKEFILE_DIR)" \
    -path "$(BUILD_HOST_DIR_ABS)" -prune -o \
    -path "$(LIB_DIR_ABS)" -prune -o \
    -name "*.a" -exec cp {} "$(LIB_DIR_ABS)" \;
	@# Ensure we use the Wii build of libpython (avoid host overwrite).
	@cp "$(BUILD_DIR_ABS)/libpython$(VERSION).a" "$(LIB_DIR_ABS)/"
	@echo "Copied libraries to $(LIB_DIR)"


build-host: $(HOST_BUILD_PYTHON)

$(HOST_BUILD_PYTHON):
	@mkdir -p "$(BUILD_HOST_DIR)"
	cd "$(BUILD_HOST_DIR)" && \
	CONFIG_SITE= ../configure --without-ensurepip && \
	: > Modules/Setup.local && \
	$(MAKE) -j$(CPU_CORES)

configure: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile: $(BUILD_PYTHON)
	@mkdir -p $(TMPDIR)
	@mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)" && \
	$(CONFIGURE_ENV) \
	../configure $(CONFIGURE_FLAGS)

libpython: configure ssl curl $(BUILD_DIR)/Modules/wiitoolsmodule.o
	@# Ensure build-wii uses our local module setup (e.g. math)
	@cmp -s "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local" 2>/dev/null || \
		cp "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local"
	$(MAKE) -j$(CPU_CORES) -C  "$(BUILD_DIR)" libpython$(VERSION).a
	@# Add wiitools to the library
	$(AR) rcs "$(BUILD_DIR)/libpython$(VERSION).a" "$(BUILD_DIR)/Modules/wiitoolsmodule.o"

python: libpython

wiitools-build: $(BUILD_DIR)/Modules/wiitoolsmodule.o

$(BUILD_DIR)/Modules/wiitoolsmodule.o: curl $(srcdir)/Modules/wiitoolsmodule.c
	@mkdir -p "$(BUILD_DIR)/Modules"
	$(CC) \
		-I$(srcdir)/Modules \
		-I$(srcdir) \
		-I$(srcdir)/Include \
		-I$(BUILD_DIR) \
		-I$(srcdir)/curl/wii/include \
		-I$(srcdir)/curl/include \
		-I$(srcdir)/curl/mbedtls/include \
		-I$(srcdir)/curl/mbedtls/wii/include \
		-I$(srcdir)/curl/mbedtls/tf-psa-crypto/include \
		-I$(srcdir)/curl/mbedtls/tf-psa-crypto/drivers/builtin/include \
		$(CFLAGS) $(CFLAGS_WII) -DPy_BUILD_CORE \
		-c "$(srcdir)/Modules/wiitoolsmodule.c" \
		-o "$(BUILD_DIR)/Modules/wiitoolsmodule.o"

ssl: configure
	$(MAKE) -j$(CPU_CORES) -C "$(BUILD_DIR)" mbedtls-wii

curl: ssl
	$(MAKE) -j$(CPU_CORES) -C "$(BUILD_DIR)" curl-wii

regen-importlib:
	@PYTHON_FOR_REGEN=$${PYTHON_FOR_REGEN:-/usr/bin/python3}; \
	if [ ! -x "$$PYTHON_FOR_REGEN" ]; then \
		echo "PYTHON_FOR_REGEN not found: $$PYTHON_FOR_REGEN"; \
		exit 1; \
	fi; \
	if [ ! -f "$(BUILD_HOST_DIR)/Makefile" ]; then \
		mkdir -p "$(BUILD_HOST_DIR)"; \
		cd "$(BUILD_HOST_DIR)" && CONFIG_SITE= ../configure --without-ensurepip && : > Modules/Setup.local; \
	fi; \
	$(MAKE) -j$(CPU_CORES) -C "$(BUILD_HOST_DIR)" PYTHON_FOR_REGEN="$$PYTHON_FOR_REGEN" regen-importlib

py: 
	@if [ -x "$(HOST_BUILD_PYTHON)" ]; then \
		echo "Using build-host python: $(HOST_BUILD_PYTHON)"; \
		$(MAKE) BUILD_PYTHON="$(HOST_BUILD_PYTHON)" cp-libs; \
	elif [ -n "$(SYSTEM_PYTHON3)" ]; then \
		echo "Using system python3: $(SYSTEM_PYTHON3)"; \
		$(MAKE) BUILD_PYTHON="$(SYSTEM_PYTHON3)" cp-libs; \
	else \
		echo "python3 not found and build-host/python missing"; \
		exit 1; \
	fi

install: py
	@if [ "$$(id -u)" != "0" ]; then \
		echo ""; \
		echo "please try "sudo make install""; \
		echo ""; \
		exit 1; \
	fi
	@mkdir -p $(DEVKITPRO)/portlibs/ppc/lib
	@mkdir -p $(DEVKITPRO)/portlibs/ppc/include/Python
	
	@cp -r libs/* $(DEVKITPRO)/portlibs/ppc/lib
	@cp -r Include/* $(DEVKITPRO)/portlibs/ppc/include/Python
	@echo "Installation complete."

wiitest: py
	@if [ -d "wiitest" ]; then \
		cd wiitest && $(MAKE) clean && $(MAKE) -j$(CPU_CORES); \
	else \
		echo "wiitest/ not found, skipping"; \
	fi

wiitest-clean:
	@$(MAKE) -C $(MAKEFILE_DIR)wiitest clean

fat-clean:
	@$(MAKE) -C $(MAKEFILE_DIR)fat ogc-clean

fat:
	$(MAKE) -j$(CPU_CORES) -C $(MAKEFILE_DIR)fat wii-release

clean: fat-clean wiitest-clean bitmap-clean
	@-rm -rf "$(BUILD_DIR)"
	@-rm -rf $(MAKEFILE_DIR)bitmap/build
	@-rm -rf $(LIB_DIR)
	@echo "cleaning ..."
