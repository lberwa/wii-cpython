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
HOST_VENV_DIR    := $(abspath $(BUILD_HOST_DIR))/venv
HOST_VENV_PYTHON := $(HOST_VENV_DIR)/bin/python
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
WII_DEFINES = \
	-DWII_BUILD \
	-D__WII__ \
	-D__wii__ \
	-D__PPC__ \
	-D__powerpc__ \
	-DWII_LIBOGC=$(LIBOGC) \
	#-DTERMINAL_PRINT_DEBUG
LIBOGC ?= 1
ifeq ($(LIBOGC),2)
  LIBOGC_INC ?= $(DEVKITPRO)/libogc2/wii/include
  LIBOGC_LIB ?= $(DEVKITPRO)/libogc2/wii/lib
else
  LIBOGC_INC ?= $(DEVKITPRO)/libogc/include
  LIBOGC_LIB ?= $(DEVKITPRO)/libogc/lib/wii
endif
export LIBOGC
export LIBOGC_INC
export LIBOGC_LIB

WII_INCLUDE_DIRS := \
	-I. \
	-I$(MAKEFILE_DIR)bitmap/include \
	-I$(MAKEFILE_DIR)curl/wii/include \
	-I$(LIBOGC_INC) \
	-I$(LIBOGC_INC)/ogc \
	-I$(DEVKITPRO)/portlibs/ppc/include
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
	ac_cv_header_sys_statvfs_h=no ac_cv_func_statvfs=no \
	DYNLOADFILE="dynload_wii.o"

CONFIGURE_FLAGS= \
	--host=powerpc-eabi --build=$$(../config.guess) \
	--with-build-python="$(BUILD_PYTHON)" \
	--without-ensurepip --disable-shared --disable-ipv6 --with-mimalloc=no

FROZEN_HEADERS := $(shell sed -n 's/^#include "frozen_modules\/\(.*\)"/Python\/frozen_modules\/\1/p' Python/frozen.c)

.PHONY: all clean configure libpython build-host python curl ssl wiitest	\
 		regen-importlib frozen-modules bitmap bitmap-clean wiitest-clean py \
 		gdbm gdbm-clean xz xz-clean uuid uuid-clean glibc glibc-clean \
 		pip-sd pip-sd-clean so-modules so-modules-clean

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

# --- Wii dlopen loader (Ersatz fuer glibc-dlfcn/ld.so) ---
# Baut den PPC-ELF-Runtime-Loader (dlfcn/wii_dlfcn.c) zu libwiidl.a.
# Der Ordner glibc/ enthaelt nur den Upstream-Quellbaum als Referenz
# (PPC-Relocation-Mathematik) und wird NICHT als Ganzes gebaut.
GLIBC_DIR := $(MAKEFILE_DIR)dlfcn
GLIBC_LIB := $(GLIBC_DIR)/libwiidl.a
GLIBC_OBJ := $(GLIBC_DIR)/wii_dlfcn.o

glibc: $(GLIBC_LIB)

$(GLIBC_LIB): $(GLIBC_DIR)/wii_dlfcn.c $(GLIBC_DIR)/wii_dlfcn.h
	$(CC) $(CFLAGS) $(CFLAGS_WII) -c "$(GLIBC_DIR)/wii_dlfcn.c" -o "$(GLIBC_OBJ)"
	$(AR) rcs "$(GLIBC_LIB)" "$(GLIBC_OBJ)"

glibc-clean:
	@-rm -f "$(GLIBC_OBJ)" "$(GLIBC_LIB)"

cp-libs: python curl bitmap gdbm xz uuid glibc
	@rm -rf "$(LIB_DIR)"
	@mkdir -p "$(LIB_DIR)"
	@# Der Upstream-glibc-Quellbaum (glibc/) enthaelt ASCII-Linkerskripte
	@# (glibc/htl/libpthread*.a), die keine echten Wii-Archive sind -> ausschliessen.
	@find "$(MAKEFILE_DIR)" \
    -path "$(BUILD_HOST_DIR_ABS)" -prune -o \
    -path "$(LIB_DIR_ABS)" -prune -o \
    -path "$(MAKEFILE_DIR)glibc" -prune -o \
    -name "*.a" -exec cp {} "$(LIB_DIR_ABS)" \;
	@# Ensure we use the Wii build of libpython (avoid host overwrite).
	@cp "$(BUILD_DIR_ABS)/libpython$(VERSION).a" "$(LIB_DIR_ABS)/"
	@echo "Copied libraries to $(LIB_DIR)"


build-host: $(HOST_VENV_PYTHON)

$(HOST_BUILD_PYTHON):
	@mkdir -p "$(BUILD_HOST_DIR)"
	cd "$(BUILD_HOST_DIR)" && \
	CONFIG_SITE= ../configure && \
	: > Modules/Setup.local && \
	$(MAKE) -j$(CPU_CORES)
	@"$(HOST_BUILD_PYTHON)" -c "import ssl" 2>/dev/null || \
	    { echo "ERROR: build-host/python was built without SSL. Run 'sudo apt install libssl-dev' then 'rm -rf build-host && make build-host'."; exit 1; }

$(HOST_VENV_PYTHON): $(HOST_BUILD_PYTHON)
	@echo "--- Erstelle venv in $(HOST_VENV_DIR) und installiere jinja2 ---"
	@rm -rf "$(HOST_VENV_DIR)"
	"$(HOST_BUILD_PYTHON)" -m venv "$(HOST_VENV_DIR)"
	"$(HOST_VENV_PYTHON)" -m pip install --quiet jinja2

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
	@# Apply Wii-specific build-wii/Makefile patches (module build rules).
	@$(MAKE) -f $(MAKEFILE_DIR)Makefile wii-patch-makefile BUILD_DIR="$(BUILD_DIR)"
	@# Prevent build-wii's make from re-running config.status (which would
	@# overwrite pyconfig.h).  Touch Makefile.pre so it is newer than both
	@# Makefile.pre.in and config.status.
	@touch "$(BUILD_DIR)/Makefile.pre"

frozen-modules:
	@missing=0; \
	for file in $(FROZEN_HEADERS); do \
		bfile="$(BUILD_DIR)/Python/frozen_modules/$$(basename $$file)"; \
		if [ ! -f "$$file" ] && [ ! -f "$$bfile" ]; then \
			echo "missing frozen header: $$file"; \
			missing=1; \
			break; \
		fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then \
		$(MAKE) regen-importlib; \
	fi

libpython: configure frozen-modules ssl curl $(BUILD_DIR)/Modules/wiitoolsmodule.o
	@# Ensure build-wii uses our local module setup (e.g. math)
	@cmp -s "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local" 2>/dev/null || \
		cp "$(srcdir)/Modules/Setup.local" "$(BUILD_DIR)/Modules/Setup.local"
	@# configure runs makesetup with an empty Setup.local, producing a Makefile
	@# with MODOBJS=[] and a config.c that references only bootstrap modules.
	@# With -j8, the archive rule can complete with those empty MODOBJS before
	@# make restarts after detecting that our Setup.local caused Makefile to be
	@# regenerated -- leaving all PyInit_* symbols out of libpython.a.
	@# Fix: delete config.c here so makesetup is forced to run as the very first
	@# step (single-threaded) and produce both a correct Makefile and config.c
	@# before any compilation starts.
	@rm -f "$(BUILD_DIR)/Modules/config.c"
	@$(MAKE) -j1 -C "$(BUILD_DIR)" Modules/config.c
	@# Re-apply Wii-specific patches (makesetup just overwrote Makefile).
	@$(MAKE) -f $(MAKEFILE_DIR)Makefile wii-patch-makefile BUILD_DIR="$(BUILD_DIR)"
	@touch "$(BUILD_DIR)/Makefile"
	@# Interrupted builds can leave behind empty object files that make treats as
	@# up-to-date. Drop them so they are rebuilt before archiving/linking.
	@find "$(BUILD_DIR)" -name '*.o' -size 0 -print -delete 2>/dev/null || true
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
	@# Compile and add Wii socket stubs (functions declared in libogc but not implemented)
	$(CC) $(CFLAGS) $(CFLAGS_WII) -I"$(srcdir)/Include" -I"$(BUILD_DIR)" -DPy_BUILD_CORE \
		-c "$(srcdir)/Modules/wii_socket_stubs.c" \
		-o "$(BUILD_DIR)/Modules/wii_socket_stubs.o"
	$(AR) rcs "$(BUILD_DIR)/libpython$(VERSION).a" "$(BUILD_DIR)/Modules/wii_socket_stubs.o"

python: libpython

# WII_BUILD-only frozen modules (not regenerated by regen-importlib)
# Wii-specific frozen modules (kept small — SD card loads the rest via WiiSourceLoader).
# Large/optional groups (asyncio, email, http, urllib, logging, xml, compression,
# zipfile, tarfile, argparse, dataclasses, importlib.metadata/resources, etc.)
# are NOT frozen — they live as .py files in wii-folder/python/ on the SD card.
WII_FROZEN_MODS := pkgutil enum keyword operator copyreg reprlib warnings \
    _py_warnings threading weakref _weakrefset copy _compat_pickle \
    struct zoneinfo zoneinfo._tzpath zoneinfo._common \
    signal base64 socket ssl selectors locale queue contextlib \
    string string.templatelib \
    hashlib \
    json json.decoder json.encoder json.scanner \
    ipaddress bisect \
    tempfile shutil fnmatch glob \
    pathlib pathlib._local pathlib._os pathlib.types \
    random datetime

WII_FROZEN_OUT := $(patsubst %,$(BUILD_DIR)/Python/frozen_modules/%.h,$(WII_FROZEN_MODS))

WII_FROZEN_SRC_pkgutil             := Lib/pkgutil.py
WII_FROZEN_SRC_enum                := Lib/enum.py
WII_FROZEN_SRC_keyword             := Lib/keyword.py
WII_FROZEN_SRC_operator            := Lib/operator.py
WII_FROZEN_SRC_copyreg             := Lib/copyreg.py
WII_FROZEN_SRC_reprlib             := Lib/reprlib.py
WII_FROZEN_SRC_warnings            := Lib/warnings.py
WII_FROZEN_SRC__py_warnings        := Lib/_py_warnings.py
WII_FROZEN_SRC_threading           := Lib/threading.py
WII_FROZEN_SRC_weakref             := Lib/weakref.py
WII_FROZEN_SRC__weakrefset         := Lib/_weakrefset.py
WII_FROZEN_SRC_copy                := Lib/copy.py
WII_FROZEN_SRC__compat_pickle      := Lib/_compat_pickle.py
WII_FROZEN_SRC_struct              := Lib/struct.py
WII_FROZEN_SRC_zoneinfo            := Lib/zoneinfo/__init__.py
WII_FROZEN_SRC_zoneinfo._tzpath    := Lib/zoneinfo/_tzpath.py
WII_FROZEN_SRC_zoneinfo._common    := Lib/zoneinfo/_common.py
WII_FROZEN_SRC_signal              := Lib/signal.py
WII_FROZEN_SRC_base64              := Lib/base64.py
WII_FROZEN_SRC_socket              := Lib/socket.py
WII_FROZEN_SRC_ssl                 := Lib/ssl.py
WII_FROZEN_SRC_selectors           := Lib/selectors.py
WII_FROZEN_SRC_locale              := Lib/locale.py
WII_FROZEN_SRC_queue               := Lib/queue.py
WII_FROZEN_SRC_contextlib          := Lib/contextlib.py
WII_FROZEN_SRC_string              := Lib/string/__init__.py
WII_FROZEN_SRC_string.templatelib  := Lib/string/templatelib.py
WII_FROZEN_SRC_hashlib             := Lib/hashlib.py
WII_FROZEN_SRC_json                := Lib/json/__init__.py
WII_FROZEN_SRC_json.decoder        := Lib/json/decoder.py
WII_FROZEN_SRC_json.encoder        := Lib/json/encoder.py
WII_FROZEN_SRC_json.scanner        := Lib/json/scanner.py
WII_FROZEN_SRC_ipaddress           := Lib/ipaddress.py
WII_FROZEN_SRC_bisect              := Lib/bisect.py
WII_FROZEN_SRC_tempfile            := Lib/tempfile.py
WII_FROZEN_SRC_shutil              := Lib/shutil.py
WII_FROZEN_SRC_fnmatch             := Lib/fnmatch.py
WII_FROZEN_SRC_glob                := Lib/glob.py
WII_FROZEN_SRC_pathlib             := Lib/pathlib/__init__.py
WII_FROZEN_SRC_pathlib._local      := Lib/pathlib/_local.py
WII_FROZEN_SRC_pathlib._os         := Lib/pathlib/_os.py
WII_FROZEN_SRC_pathlib.types       := Lib/pathlib/types.py
WII_FROZEN_SRC_random              := Lib/random.py
WII_FROZEN_SRC_datetime            := Lib/datetime.py

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

# Idempotently patch build-wii/Makefile with Wii-specific module build rules.
# Marker: WII_MAKEFILE_PATCHED in a comment at the top of the Makefile.
.PHONY: wii-patch-makefile
wii-patch-makefile:
	@if grep -q 'WII_MAKEFILE_PATCHED' "$(BUILD_DIR)/Makefile" 2>/dev/null; then exit 0; fi; \
	echo "Patching $(BUILD_DIR)/Makefile with Wii module build rules"; \
	sed -i 's|\(Modules/selectmodule_wii\.o:.*\); \$$(CC)  -I\$$(LIBOGC_INC) -DHAVE_SELECT|\1; $$(CC)  -I$$(abs_srcdir)/curl/wii/include -I$$(LIBOGC_INC) -DHAVE_SELECT|' \
	    "$(BUILD_DIR)/Makefile"; \
	sed -i 's|-DHAVE_GETADDRINFO -DENABLE_IPV6 |-DHAVE_GETADDRINFO -DHAVE_GETNAMEINFO |g' \
	    "$(BUILD_DIR)/Makefile"; \
	sed -i 's|-I\$$(abs_srcdir)/curl/wii/include -I\$$(LIBOGC_INC) -DHAVE_SOCKET|-I$$(abs_srcdir)/curl/wii/include -I$$(LIBOGC_INC) -I$$(LIBOGC_INC)/ogc -include $$(abs_srcdir)/curl/wii/include/curl_wii_net_compat.h -DHAVE_SOCKET|' \
	    "$(BUILD_DIR)/Makefile"; \
	echo '# WII_MAKEFILE_PATCHED' >> "$(BUILD_DIR)/Makefile"

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
	@test -x "$(HOST_VENV_PYTHON)" || \
	    { echo "ERROR: $(HOST_VENV_DIR) not found. Please run: make build-host"; exit 1; }
	$(MAKE) -j$(CPU_CORES) -C "$(BUILD_DIR)" mbedtls-wii PYTHON="$(HOST_VENV_PYTHON)"

curl: ssl
	$(MAKE) -j$(CPU_CORES) -C "$(BUILD_DIR)" curl-wii PYTHON="$(HOST_VENV_PYTHON)"

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
	$(MAKE) pip-sd
	$(MAKE) so-modules

install: py
	@if [ "$$(id -u)" != "0" ]; then \
		echo ""; \
		echo "please try \"sudo make install\""; \
		echo ""; \
		exit 1; \
	fi
	@mkdir -p $(DEVKITPRO)/portlibs/ppc/lib
	@mkdir -p $(DEVKITPRO)/portlibs/ppc/include/Python
	@mkdir -p $(DEVKITPRO)/portlibs/ppc/include/curl

	@cp -r libs/* $(DEVKITPRO)/portlibs/ppc/lib
	@cp -r Include/* $(DEVKITPRO)/portlibs/ppc/include/Python
	@cp "$(BUILD_DIR)/pyconfig.h" $(DEVKITPRO)/portlibs/ppc/include/Python/pyconfig.h
	@cp curl/include/curl/*.h $(DEVKITPRO)/portlibs/ppc/include/curl/

	@chmod a+w libs/

	@echo "Installation complete."

remove:
	@if [ "$$(id -u)" != "0" ]; then \
		echo ""; \
		echo "please try \"sudo make remove\""; \
		echo ""; \
		exit 1; \
	fi
	@rm -rf $(DEVKITPRO)/portlibs/ppc/include/Python
	@rm -rf $(DEVKITPRO)/portlibs/ppc/include/curl
	@if [ -d "$(LIB_DIR)" ]; then \
		for f in "$(LIB_DIR)"/*.a; do \
			rm -f "$(DEVKITPRO)/portlibs/ppc/lib/$$(basename $$f)"; \
		done; \
	else \
		echo "Warning: $(LIB_DIR)/ not found, run 'make py' first to remove .a files accurately"; \
	fi
	@echo "Removal complete."

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

# ---------------------------------------------------------------------------
# pip-sd: populate wii-folder/python/ with pip + stdlib for the SD card.
#
#   pip is extracted from the bundled wheel as individual .py files.
#   Lib/ Python files are copied, excluding GUI / test / cache dirs.
#
# Copy wii-folder/python/ -> sd:/python/ on the Wii SD card.
# sys.path already includes sd:/python, so everything is importable.
#
# Usage:
#   make pip-sd        -- build wii-folder/ (idempotent)
#   make pip-sd-clean  -- remove wii-folder/
# ---------------------------------------------------------------------------
WII_FOLDER := $(MAKEFILE_DIR)wii-folder

pip-sd:
	@python3 "$(MAKEFILE_DIR)Tools/wii/pip_sd.py"

pip-sd-clean:
	@echo "Removing wii-folder/..."
	@rm -rf "$(WII_FOLDER)"
	@echo "pip-sd-clean: done"

# ---------------------------------------------------------------------------
# so-modules: build non-essential C extension modules as .so files for SD card.
# These were removed from Setup.local (and thus from libpython.a) to shrink
# boot.dol. WiiSourceFinder loads them via dlopen from sd:/python/<name>.so.
#
# Flags: -fPIC -fno-plt -shared -nostdlib (same as spam.so in wiitest).
# Symbol resolution: the main .elf exports all Python API symbols.
# ---------------------------------------------------------------------------

SO_OUT_DIR  := $(WII_FOLDER)/python
SO_CC       := $(CC)
SO_CFLAGS   := -Os -fPIC -Wall -DWII_BUILD -D__WII__ -D__wii__ \
               -I$(MAKEFILE_DIR)build-wii \
               -I$(MAKEFILE_DIR)Include \
               -I$(MAKEFILE_DIR)Include/internal \
               -I$(MAKEFILE_DIR)Modules \
               $(WII_INCLUDE_DIRS)
SO_LDFLAGS  := -shared -nostdlib -lgcc -Wl,-z,notext

MPDEC_SRCS := \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/basearith.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/constants.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/context.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/convolute.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/crt.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/difradix2.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/fnt.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/fourstep.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/io.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/mpalloc.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/mpdecimal.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/mpsignal.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/numbertheory.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/sixstep.c \
    $(MAKEFILE_DIR)Modules/_decimal/libmpdec/transpose.c

SQLITE_SRCS := \
    $(MAKEFILE_DIR)Modules/_sqlite/blob.c \
    $(MAKEFILE_DIR)Modules/_sqlite/connection.c \
    $(MAKEFILE_DIR)Modules/_sqlite/cursor.c \
    $(MAKEFILE_DIR)Modules/_sqlite/microprotocols.c \
    $(MAKEFILE_DIR)Modules/_sqlite/module.c \
    $(MAKEFILE_DIR)Modules/_sqlite/prepare_protocol.c \
    $(MAKEFILE_DIR)Modules/_sqlite/row.c \
    $(MAKEFILE_DIR)Modules/_sqlite/statement.c \
    $(MAKEFILE_DIR)Modules/_sqlite/util.c

CJKCODECS_DIR := $(MAKEFILE_DIR)Modules/cjkcodecs

SO_TARGETS := \
    $(SO_OUT_DIR)/_bisect.so \
    $(SO_OUT_DIR)/_random.so \
    $(SO_OUT_DIR)/_statistics.so \
    $(SO_OUT_DIR)/cmath.so \
    $(SO_OUT_DIR)/_csv.so \
    $(SO_OUT_DIR)/_lsprof.so \
    $(SO_OUT_DIR)/_asyncio.so \
    $(SO_OUT_DIR)/_queue.so \
    $(SO_OUT_DIR)/_heapq.so \
    $(SO_OUT_DIR)/array.so \
    $(SO_OUT_DIR)/xxsubtype.so \
    $(SO_OUT_DIR)/xxlimited.so \
    $(SO_OUT_DIR)/xxlimited_35.so \
    $(SO_OUT_DIR)/_datetime.so \
    $(SO_OUT_DIR)/_decimal.so \
    $(SO_OUT_DIR)/_sqlite3.so \
    $(SO_OUT_DIR)/_multibytecodec.so \
    $(SO_OUT_DIR)/_codecs_cn.so \
    $(SO_OUT_DIR)/_codecs_hk.so \
    $(SO_OUT_DIR)/_codecs_jp.so \
    $(SO_OUT_DIR)/_codecs_kr.so \
    $(SO_OUT_DIR)/_codecs_tw.so \
    $(SO_OUT_DIR)/_codecs_iso2022.so \
    $(SO_OUT_DIR)/_ctypes.so

SO_PY_TARGETS := \
    $(SO_OUT_DIR)/_hashlib.py

so-modules: $(SO_TARGETS) $(SO_PY_TARGETS)
	@echo "so-modules: $(words $(SO_TARGETS)) .so + $(words $(SO_PY_TARGETS)) .py -> $(SO_OUT_DIR)"

$(SO_OUT_DIR)/_bisect.so: $(MAKEFILE_DIR)Modules/_bisectmodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_random.so: $(MAKEFILE_DIR)Modules/_randommodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_statistics.so: $(MAKEFILE_DIR)Modules/_statisticsmodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/cmath.so: $(MAKEFILE_DIR)Modules/cmathmodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_csv.so: $(MAKEFILE_DIR)Modules/_csv.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_lsprof.so: $(MAKEFILE_DIR)Modules/_lsprof.c $(MAKEFILE_DIR)Modules/rotatingtree.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    $(MAKEFILE_DIR)Modules/_lsprof.c $(MAKEFILE_DIR)Modules/rotatingtree.c -o $@

$(SO_OUT_DIR)/_asyncio.so: $(MAKEFILE_DIR)Modules/_asynciomodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_queue.so: $(MAKEFILE_DIR)Modules/_queuemodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_heapq.so: $(MAKEFILE_DIR)Modules/_heapqmodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/array.so: $(MAKEFILE_DIR)Modules/arraymodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/xxsubtype.so: $(MAKEFILE_DIR)Modules/xxsubtype.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

# xxlimited defines Py_LIMITED_API itself — do not pass Py_BUILD_CORE_MODULE
$(SO_OUT_DIR)/xxlimited.so: $(MAKEFILE_DIR)Modules/xxlimited.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) $< -o $@

$(SO_OUT_DIR)/xxlimited_35.so: $(MAKEFILE_DIR)Modules/xxlimited_35.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) $< -o $@

$(SO_OUT_DIR)/_datetime.so: $(MAKEFILE_DIR)Modules/_datetimemodule.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE $< -o $@

$(SO_OUT_DIR)/_decimal.so: $(MAKEFILE_DIR)Modules/_decimal/_decimal.c $(MPDEC_SRCS)
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -DCONFIG_32 -DANSI \
	    -I$(MAKEFILE_DIR)Modules/_decimal \
	    -I$(MAKEFILE_DIR)Modules/_decimal/libmpdec \
	    $(MAKEFILE_DIR)Modules/_decimal/_decimal.c $(MPDEC_SRCS) -o $@

$(SO_OUT_DIR)/_sqlite3.so: $(SQLITE_SRCS) $(MAKEFILE_DIR)sqlite/sqlite3.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -DSQLITE_OMIT_LOAD_EXTENSION -DMODULE_NAME='"sqlite3"' \
	    -DSQLITE_OMIT_WAL -DSQLITE_OMIT_MMAP \
	    -I$(MAKEFILE_DIR)sqlite \
	    $(SQLITE_SRCS) $(MAKEFILE_DIR)sqlite/sqlite3.c -o $@

# _multibytecodec: the framework module (PyInit__multibytecodec lives here)
$(SO_OUT_DIR)/_multibytecodec.so: $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $< -o $@

# Each codec .so bundles multibytecodec.c to resolve internal symbols
# (Wii dlopen resolves only against the main .elf, not other .so files).
$(SO_OUT_DIR)/_codecs_cn.so: $(CJKCODECS_DIR)/_codecs_cn.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_codecs_hk.so: $(CJKCODECS_DIR)/_codecs_hk.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_codecs_jp.so: $(CJKCODECS_DIR)/_codecs_jp.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_codecs_kr.so: $(CJKCODECS_DIR)/_codecs_kr.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_codecs_tw.so: $(CJKCODECS_DIR)/_codecs_tw.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_codecs_iso2022.so: $(CJKCODECS_DIR)/_codecs_iso2022.c $(CJKCODECS_DIR)/multibytecodec.c
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(CJKCODECS_DIR) $^ -o $@

$(SO_OUT_DIR)/_hashlib.py: $(MAKEFILE_DIR)Modules/_hashlib_wii.py
	@mkdir -p $(SO_OUT_DIR)
	cp $< $@

FFI_DIR     := $(MAKEFILE_DIR)libffi
FFI_BUILD   := $(FFI_DIR)/build-wii
FFI_INSTALL := $(FFI_DIR)/install-wii
FFI_LIB     := $(FFI_INSTALL)/lib/libffi.a

CTYPES_SRCS := \
    $(MAKEFILE_DIR)Modules/_ctypes/_ctypes.c \
    $(MAKEFILE_DIR)Modules/_ctypes/callbacks.c \
    $(MAKEFILE_DIR)Modules/_ctypes/callproc.c \
    $(MAKEFILE_DIR)Modules/_ctypes/stgdict.c \
    $(MAKEFILE_DIR)Modules/_ctypes/cfield.c

$(FFI_LIB):
	@[ -f $(FFI_DIR)/configure ] || (cd $(FFI_DIR) && autoreconf -fi)
	@mkdir -p $(FFI_BUILD)
	cd $(FFI_BUILD) && \
	CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" \
	CFLAGS="-Os -fPIC -D__wii__ -DGEKKO -I$(LIBOGC_INC)" \
	$(FFI_DIR)/configure \
	    --host=powerpc-eabi \
	    --build=$$($(FFI_DIR)/config.guess) \
	    --prefix="$(FFI_INSTALL)" \
	    --disable-shared --enable-static --disable-docs \
	    ac_cv_func_mmap_fixed_mapped=no && \
	$(MAKE) -j$(CPU_CORES) -C $(FFI_BUILD) && \
	$(MAKE) install -C $(FFI_BUILD)

$(SO_OUT_DIR)/_ctypes.so: $(CTYPES_SRCS) $(FFI_LIB)
	@mkdir -p $(SO_OUT_DIR)
	$(SO_CC) $(SO_CFLAGS) $(SO_LDFLAGS) -DPy_BUILD_CORE_MODULE \
	    -I$(FFI_INSTALL)/include \
	    -I$(MAKEFILE_DIR)dlfcn \
	    $(CTYPES_SRCS) $(FFI_LIB) -o $@

libffi-clean:
	@-rm -rf "$(FFI_BUILD)" "$(FFI_INSTALL)"

so-modules-clean:
	@rm -f $(SO_TARGETS) $(SO_PY_TARGETS)
	@echo "so-modules-clean: done"

clean: wiitest-clean bitmap-clean glibc-clean pip-sd-clean libffi-clean
	@-rm -rf "$(BUILD_DIR)"
	@-rm -rf $(MAKEFILE_DIR)bitmap/build
	@-rm -rf $(LIB_DIR)
	@echo "cleaning ..."
