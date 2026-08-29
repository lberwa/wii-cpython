# Wii-only build. Uses Python 3.15 configure output to build libpython.a.

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

OPT=			-mhard-float -g -Os -Wall -Wstrict-prototypes -fPIC -fdata-sections -ffunction-sections
CFLAGS=			$(OPT)
WII_DEFINES := \
	-DWII_BUILD \
	-D__WII__ \
	-D__wii__ \
	-D__PPC__ \
	-D__powerpc__ \
	-DWII_SINGLE_THREAD=1 \
	#-DTERMINAL_PRINT_DEBUG
WII_INCLUDE_DIRS := \
	-I. \
	-I$(MAKEFILE_DIR)bitmap/include \
	-I$(MAKEFILE_DIR)fat/include \
	-I$(DEVKITPRO)/libogc/include \
	-I$(DEVKITPRO)/libogc/include/ogc \
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
 		regen-importlib bitmap bitmap-clean fat fat-clean wiitest-clean py \
 		gdbm gdbm-clean xz xz-clean uuid uuid-clean

all: wiitest

bitmap:
	$(MAKE) -j$(CPU_CORES) -C $(MAKEFILE_DIR)bitmap

bitmap-clean:
	@$(MAKE) -C $(MAKEFILE_DIR)bitmap clean

GDBM_DIR     := $(MAKEFILE_DIR)gdbm
GDBM_SRC     := $(GDBM_DIR)/src
GDBM_BUILD   := $(GDBM_DIR)/build-wii
GDBM_INSTALL := $(GDBM_DIR)/install-wii

gdbm: $(GDBM_INSTALL)/lib/libgdbm.a

$(GDBM_INSTALL)/lib/libgdbm.a:
	@mkdir -p "$(GDBM_BUILD)"
	cd "$(GDBM_BUILD)" && \
	CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" \
	CFLAGS="$(CFLAGS) $(CFLAGS_WII)" \
	"$(GDBM_SRC)/configure" \
		--host=powerpc-eabi \
		--build=$$($(GDBM_SRC)/build-aux/config.guess) \
		--prefix="$(GDBM_INSTALL)" \
		--enable-libgdbm-compat \
		--disable-shared \
		--enable-static \
		--disable-nls \
		--without-readline \
		--disable-memory-mapped-io \
		ac_cv_func_flock=no \
		ac_cv_func_lockf=no && \
	$(MAKE) -j$(CPU_CORES) -C "$(GDBM_BUILD)/src" && \
	$(MAKE) install -C "$(GDBM_BUILD)/src" && \
	$(MAKE) -j$(CPU_CORES) -C "$(GDBM_BUILD)/compat" && \
	$(MAKE) install -C "$(GDBM_BUILD)/compat"

gdbm-clean:
	@-rm -rf "$(GDBM_BUILD)" "$(GDBM_INSTALL)"

XZ_DIR     := $(MAKEFILE_DIR)xz
XZ_SRC     := $(XZ_DIR)/src
XZ_BUILD   := $(XZ_DIR)/build-wii
XZ_INSTALL := $(XZ_DIR)/install-wii

xz: $(XZ_INSTALL)/lib/liblzma.a

$(XZ_INSTALL)/lib/liblzma.a:
	@mkdir -p "$(XZ_BUILD)"
	cd "$(XZ_BUILD)" && \
	CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" \
	CFLAGS="$(CFLAGS) $(CFLAGS_WII)" \
	"$(XZ_SRC)/configure" \
		--host=powerpc-eabi \
		--build=$$($(XZ_SRC)/build-aux/config.guess) \
		--prefix="$(XZ_INSTALL)" \
		--disable-shared \
		--enable-static \
		--disable-nls \
		--disable-xz \
		--disable-xzdec \
		--disable-lzmadec \
		--disable-lzmainfo \
		--disable-lzma-links \
		--disable-scripts \
		--disable-doc \
		--disable-threads \
		ac_cv_func_futimens=no \
		ac_cv_func_clock_gettime=no && \
	$(MAKE) -j$(CPU_CORES) -C "$(XZ_BUILD)/src/liblzma" && \
	$(MAKE) install -C "$(XZ_BUILD)/src/liblzma"

xz-clean:
	@-rm -rf "$(XZ_BUILD)" "$(XZ_INSTALL)"

UUID_DIR     := $(MAKEFILE_DIR)uuid
UUID_SRC     := $(UUID_DIR)/src
UUID_BUILD   := $(UUID_DIR)/build-wii
UUID_INSTALL := $(UUID_DIR)/install-wii

uuid: $(UUID_INSTALL)/lib/libuuid.a

$(UUID_INSTALL)/lib/libuuid.a:
	@mkdir -p "$(UUID_BUILD)"
	cd "$(UUID_BUILD)" && \
	CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" \
	CFLAGS="$(CFLAGS) $(CFLAGS_WII)" \
	"$(UUID_SRC)/configure" \
		--host=powerpc-eabi \
		--build=$$($(UUID_SRC)/config.guess) \
		--prefix="$(UUID_INSTALL)" \
		--disable-shared \
		--enable-static \
		ac_cv_header_net_if_h=no \
		ac_cv_header_sys_un_h=no \
		ac_cv_header_net_if_dl_h=no \
		ac_cv_func_uuidd=no \
		ac_cv_func_clock_gettime=no && \
	$(MAKE) -j$(CPU_CORES) libuuid.la && \
	$(MAKE) install-libLTLIBRARIES && \
	mkdir -p "$(UUID_INSTALL)/include/uuid" && \
	cp "$(UUID_SRC)/uuid.h" "$(UUID_INSTALL)/include/uuid/uuid.h"

uuid-clean:
	@-rm -rf "$(UUID_BUILD)" "$(UUID_INSTALL)"

cp-libs: python curl bitmap fat gdbm xz uuid
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

# order-only prerequisite (|): the build-host python only has to *exist* before
# we configure.  A plain prerequisite would re-run configure whenever build-host
# was rebuilt (newer mtime), and since configure regenerates config.status but
# not build-wii/Makefile itself, the target stayed perpetually stale -> configure
# ran on every `make py`.  We touch the Makefile so a completed configure marks
# the target up-to-date.
$(BUILD_DIR)/Makefile: | $(BUILD_PYTHON)
	@mkdir -p $(TMPDIR)
	@mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)" && \
	$(CONFIGURE_ENV) \
	../configure $(CONFIGURE_FLAGS)
	@touch "$(BUILD_DIR)/Makefile"
	@# Apply Wii-specific pyconfig.h additions once, right after configure.
	@$(MAKE) -f $(MAKEFILE_DIR)Makefile wii-patch-pyconfig BUILD_DIR="$(BUILD_DIR)"
	@# Prevent build-wii's make from re-running config.status (which would
	@# overwrite pyconfig.h).  Touch Makefile.pre so it is newer than both
	@# Makefile.pre.in and config.status.
	@touch "$(BUILD_DIR)/Makefile.pre"

libpython: configure ssl curl $(BUILD_DIR)/Modules/wiitoolsmodule.o wii-frozen-modules
	@# Guard: re-apply pyconfig.h patches if config.status wiped them,
	@# and re-stamp Makefile.pre so it won't happen again this run.
	@$(MAKE) -f $(MAKEFILE_DIR)Makefile wii-patch-pyconfig BUILD_DIR="$(BUILD_DIR)"
	@touch "$(BUILD_DIR)/Makefile.pre"
	@# Ensure build-wii uses our local module setup (e.g. math)
	@cmp -s "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local" 2>/dev/null || \
		cp "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local"
	@# frozen.o has no header dependency in the generated Makefile; invalidate it
	@# whenever frozen.c or any frozen_modules/*.h changed, so re-frozen stdlib
	@# modules actually make it into libpython.
	@if [ -f "$(BUILD_DIR)/Python/frozen.o" ] && \
	    [ -n "$$(find $(srcdir)/Python/frozen.c $(srcdir)/Python/frozen_modules -newer $(BUILD_DIR)/Python/frozen.o 2>/dev/null)" ]; then \
		echo "frozen sources changed -> rebuilding frozen.o"; \
		rm -f "$(BUILD_DIR)/Python/frozen.o"; \
	fi
	$(MAKE) -j$(CPU_CORES) -C  "$(BUILD_DIR)" libpython$(VERSION).a
	@# Add wiitools to the library
	$(AR) rcs "$(BUILD_DIR)/libpython$(VERSION).a" "$(BUILD_DIR)/Modules/wiitoolsmodule.o"

python: libpython

# WII_BUILD-only frozen modules (not regenerated by regen-importlib)
WII_FROZEN_MODS := pkgutil enum keyword operator copyreg reprlib warnings \
    _py_warnings threading weakref _weakrefset copy _compat_pickle \
    xml xml.etree xml.etree.ElementPath sysconfig struct \
    zoneinfo zoneinfo._tzpath zoneinfo._common

WII_FROZEN_OUT := $(patsubst %,$(BUILD_DIR)/Python/frozen_modules/%.h,$(WII_FROZEN_MODS))

WII_FROZEN_SRC_pkgutil             := Lib/pkgutil.py
WII_FROZEN_SRC_enum                := Lib/enum.py
WII_FROZEN_SRC_keyword             := Lib/keyword.py
WII_FROZEN_SRC_operator            := Lib/operator.py
WII_FROZEN_SRC_copyreg             := Lib/copyreg.py
WII_FROZEN_SRC_reprlib             := Lib/reprlib.py
WII_FROZEN_SRC_warnings            := Lib/warnings.py
WII_FROZEN_SRC__py_warnings        := Lib/warnings.py
WII_FROZEN_SRC_threading           := Lib/threading.py
WII_FROZEN_SRC_weakref             := Lib/weakref.py
WII_FROZEN_SRC__weakrefset         := Lib/_weakrefset.py
WII_FROZEN_SRC_copy                := Lib/copy.py
WII_FROZEN_SRC__compat_pickle      := Lib/_compat_pickle.py
WII_FROZEN_SRC_xml                 := Lib/xml/__init__.py
WII_FROZEN_SRC_xml.etree           := Lib/xml/etree/__init__.py
WII_FROZEN_SRC_xml.etree.ElementPath := Lib/xml/etree/ElementPath.py
WII_FROZEN_SRC_sysconfig           := Lib/sysconfig/__init__.py
WII_FROZEN_SRC_struct              := Lib/struct.py
WII_FROZEN_SRC_zoneinfo            := Lib/zoneinfo/__init__.py
WII_FROZEN_SRC_zoneinfo._tzpath    := Lib/zoneinfo/_tzpath.py
WII_FROZEN_SRC_zoneinfo._common    := Lib/zoneinfo/_common.py

.PHONY: wii-frozen-modules
wii-frozen-modules: $(HOST_BUILD_PYTHON)
	@mkdir -p "$(BUILD_DIR)/Python/frozen_modules"
	@$(srcdir)/Modules/wii_freeze_modules.sh \
	    "$(HOST_BUILD_PYTHON)" \
	    "$(srcdir)/Programs/_freeze_module.py" \
	    "$(srcdir)" \
	    "$(BUILD_DIR)/Python/frozen_modules"

# Idempotently patch build-wii/pyconfig.h with Wii-specific additions.
# Uses Modules/wii_pyconfig_patch.h as the content to append.
.PHONY: wii-patch-pyconfig
wii-patch-pyconfig:
	@grep -q 'WII_BSD_COMPAT_ADDED' "$(BUILD_DIR)/pyconfig.h" 2>/dev/null && exit 0 || true
	@if ! grep -q 'WII_BSD_COMPAT_ADDED' "$(BUILD_DIR)/pyconfig.h" 2>/dev/null; then \
	    echo "Patching $(BUILD_DIR)/pyconfig.h with Wii BSD compat additions"; \
	    sed -i 's|#endif /\*Py_PYCONFIG_H\*/||g' "$(BUILD_DIR)/pyconfig.h"; \
	    cat "$(srcdir)/Modules/wii_pyconfig_patch.h" >> "$(BUILD_DIR)/pyconfig.h"; \
	fi

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

	@chmod a+w libs/

	@echo "Installation complete."

wiitest: py
	@if [ -d "wiitest" ]; then \
		cd wiitest && $(MAKE) clean && $(MAKE) -j$(CPU_CORES); \
	else \
		echo "wiitest/ not found, skipping"; \
	fi

ifdef SD_APP_PATH

run: py
	@if [ -d "wiitest" ]; then \
		cd wiitest && $(MAKE) clean && $(MAKE) run SD_APP_PATH=$(SD_APP_PATH) -j$(CPU_CORES); \
	else \
		echo "wiitest/ not found, skipping"; \
	fi

else

run: py
	@if [ -d "wiitest" ]; then \
		cd wiitest && $(MAKE) clean && $(MAKE) run -j$(CPU_CORES); \
	else \
		echo "wiitest/ not found, skipping"; \
	fi

endif

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
