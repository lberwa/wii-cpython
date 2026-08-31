# load_so - laedt sd:/test.so ueber den Wii-dlopen-Loader (libwiidl.a).
#
# Baut:
#   hello_world.dol   - das Hauptprogramm (source/main.c)
#   assets/test.so    - das Beispielmodul (assets/test.c, -fno-plt -shared)
#
# Deployment: test.so gehoert auf die Root der SD-Karte als sd:/test.so.
# `make run` kopiert sowohl die .dol als auch test.so in WII_SD_SYNC
# (Dolphin-SD-Ordner). Fuer Hardware: beide Dateien selbst auf die SD legen.

export DEVKITPRO ?= /opt/devkitpro
export DEVKITPPC ?= $(DEVKITPRO)/devkitPPC

TARGET := hello_world
BUILD  := build
SRC    := source

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PPCPREFIX    := $(DEVKITPPC)/bin/powerpc-eabi

# Ordner, in den `make run` die Dateien fuer Dolphin kopiert.
WII_SD_SYNC ?= ~/Dokumente/wii/dolphin/sd-karte/WiiSDSync

include $(DEVKITPPC)/wii_rules

# Startup-Code (fuer libogc erforderlich)
CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

# Wo liegt der Loader? libwiidl.a wird von `make py` (cp-libs) nach ../../libs
# gesammelt; als Fallback direkt aus ../../dlfcn. Header aus ../../dlfcn.
CFLAGS  += -Os -g -Wall -Wextra -I$(DEVKITPRO)/libogc/include -I$(MAKEFILE_DIR)../../dlfcn

LDFLAGS += -T$(DEVKITPPC)/powerpc-eabi/lib/rvl.ld -Wl,--gc-sections
LDFLAGS += -L$(DEVKITPRO)/libogc/lib/wii
LDFLAGS += -L$(MAKEFILE_DIR)../../libs
LDFLAGS += -L$(MAKEFILE_DIR)../../dlfcn

LD := $(CC)

# -lwiidl VOR -logc (Loader nutzt DCFlushRange/ICInvalidateRange aus libogc).
LIBS := -lwiidl -lfatpy -lwiiuse -lbte -logc -lm

SRC_OFILES := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c))
MODULE     := assets/test.so

# --------------------------------------------------------------------------
.PHONY: all clean run sync

all: $(BUILD) $(MODULE) $(TARGET).dol

$(BUILD):
	@mkdir -p $(BUILD)

# Hauptprogramm
$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(SRC_OFILES)
	@test -f $(MAKEFILE_DIR)../../libs/libwiidl.a -o -f $(MAKEFILE_DIR)../../dlfcn/libwiidl.a || { \
		echo "Missing libwiidl.a - baue zuerst den Loader:  (cd ../.. && make glibc)"; \
		exit 1; \
	}
	$(LD) $(LDFLAGS) $(CRTM) $(SRC_OFILES) $(LIBS) -o $@

$(TARGET).dol: $(TARGET).elf
	@elf2dol $< $@
	@echo "gebaut: $@"

# Beispielmodul: WICHTIG -fno-plt (externe Calls -> GOT statt PLT-Stubs)
$(MODULE): assets/test.c
	$(PPCPREFIX)-gcc -Os -fPIC -fno-plt -shared -nostdlib $< -o $@
	@echo "gebaut: $@"

# Kopiert .dol + test.so nach WII_SD_SYNC (Dolphin-SD-Ordner)
sync: all
	@if [ -z "$(WII_SD_SYNC)" ] || [ ! -d "$(WII_SD_SYNC)" ]; then \
		echo "Note: WII_SD_SYNC nicht gesetzt/vorhanden - ueberspringe SD-Sync"; \
		exit 0; \
	fi
	@cp -f $(MODULE) "$(WII_SD_SYNC)/test.so" && echo "SD sync: test.so -> $(WII_SD_SYNC)/test.so"
	@cp -f $(TARGET).dol "$(WII_SD_SYNC)/boot.dol" && echo "SD sync: boot.dol -> $(WII_SD_SYNC)/boot.dol"

run: sync

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol $(MODULE)
