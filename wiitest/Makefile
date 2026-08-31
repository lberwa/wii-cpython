#CRT0 := $(DEVKITPPC)/powerpc-eabi/lib/crt0.o
CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

TARGET     := hello_world
BUILD      := build
SOURCES    := source
DATA       := data
INCLUDES   :=

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
WII_SD_SYNC ?= ~/Dokumente/wii/dolphin/sd-karte/WiiSDSync
WII_SD_SYNC_PY_GLOB := $(MAKEFILE_DIR)files/*.py

INCLUDE += -I../build-wii
INCLUDE += -I$(MAKEFILE_DIR)../build-wii
INCLUDE += -I$(MAKEFILE_DIR)../Include
INCLUDE += -I$(LIBOGC_INC)


CFLAGS     += -Os -g -std=c99 -Wall -Wextra -fdata-sections -ffunction-sections -D__wii__ -DGEKKO
CXXFLAGS   := $(CFLAGS)
CPPFLAGS   += -Os -g -D__wii__ -DGEKKO
CFLAGS     += $(INCLUDE)
CPPFLAGS   += $(INCLUDE)

# Includes & Libs
LDFLAGS += -T$(DEVKITPPC)/powerpc-eabi/lib/rvl.ld -Wl,--gc-sections

LDFLAGS    += -L$(LIBOGC_LIB)
LDFLAGS    += -L$(MAKEFILE_DIR)../libs
LDFLAGS    += -L$(MAKEFILE_DIR)..+
LDFLAGS    += -L$(MAKEFILE_DIR)../gdbm/install-wii/lib
LDFLAGS    += -L$(MAKEFILE_DIR)../xz/install-wii/lib
LDFLAGS    += -L$(MAKEFILE_DIR)../uuid/install-wii/lib

CPYTHON_BUILD := $(MAKEFILE_DIR)../libs


#LIBS       := -lpython3.15 -lfatpy -lwiiuse -lbte -lbitmap -logc -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -ltfpsacrypto
LIBS := -lpython3.15 -lwiidl -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -ltfpsacrypto -lz -lgdbm_compat -lgdbm -llzma -lbz2 -luuid -lfatpy -lwiiuse -lbte -lbitmap -logc -lm
LDFLAGS    += -L$(DEVKITPRO)/portlibs/ppc/lib


include $(DEVKITPPC)/wii_rules

# --------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT    := $(CURDIR)/$(TARGET)
export VPATH     := $(foreach dir,$(SOURCES) $(DATA),$(CURDIR)/$(dir))
export DEPSDIR   := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
PYFILES     := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.py)))
PYOBJS      := $(addprefix pydata_,$(PYFILES:.py=.o))
PNGFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.png)))
PNGOBJS     := $(addprefix pngdata_,$(PNGFILES:.png=.o))

ifeq ($(strip $(CPPFILES)),)
  export LD := $(CC)
else
  export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o) $(PYOBJS) $(PNGOBJS)

export INCLUDE += $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(BUILD)

# --- Dynamisch ladbares Python-C-Extension-Modul + Symbol-Map ---------------
# spam.so wird per `import spam` ueber den Wii-dlopen-Loader geladen.
# symbols.map ist das Wii-Aequivalent zu --export-dynamic: aus dem verlinkten
# .elf per nm erzeugt, liefert es dem Loader die Adressen der CPython-API.
PPCPREFIX  := $(DEVKITPPC)/bin/powerpc-eabi
PYMOD_SO   := spam.so
SYMBOL_MAP := symbols.map
PYMOD_SRC  := pymodules/spam.c
# WICHTIG: build-wii/pyconfig.h (echte generierte Config, gleiche ABI wie
# libpython) - NICHT die Wurzel-pyconfig.h (die erzwingt Py_BUILD_CORE und
# hat widerspruechliche Makros). Kein -I.. auf die Projektwurzel.
PYMOD_INC  := -I$(MAKEFILE_DIR)../build-wii -I$(MAKEFILE_DIR)../Include

.PHONY: $(BUILD) clean sync_sd run deploy_so
.DEFAULT_GOAL := $(BUILD)

$(PYMOD_SO): $(PYMOD_SRC)
	$(PPCPREFIX)-gcc -Os -fPIC -fno-plt -shared -nostdlib $(PYMOD_INC) $< -o $@
	@echo "gebaut: $@"

$(SYMBOL_MAP): $(OUTPUT).elf
	$(PPCPREFIX)-nm --defined-only $(OUTPUT).elf > $@
	@echo "erzeugt: $@ ($$(wc -l <$@) Symbole)"

# Kopiert spam.so + symbols.map nach WII_SD_SYNC (Dolphin-SD-Ordner).
deploy_so: $(PYMOD_SO) $(SYMBOL_MAP)
	@if [ -n "$(WII_SD_SYNC)" ] && [ -d "$(WII_SD_SYNC)" ] && [ -w "$(WII_SD_SYNC)" ]; then \
		cp -f $(PYMOD_SO) "$(WII_SD_SYNC)/$(PYMOD_SO)"; \
		cp -f $(SYMBOL_MAP) "$(WII_SD_SYNC)/$(SYMBOL_MAP)"; \
		echo "SD sync: $(PYMOD_SO) + $(SYMBOL_MAP) -> $(WII_SD_SYNC)"; \
	else \
		echo "Note: WII_SD_SYNC nicht gesetzt/schreibbar - $(PYMOD_SO)/$(SYMBOL_MAP) nicht kopiert"; \
	fi

sync_sd:
	@if [ -z "$(WII_SD_SYNC)" ] || [ ! -d "$(WII_SD_SYNC)" ]; then \
		echo "Note: skipping SD sync (set WII_SD_SYNC to an existing directory)"; \
		exit 0; \
	fi
	@if [ ! -w "$(WII_SD_SYNC)" ]; then \
		echo "Note: skipping SD sync ($(WII_SD_SYNC) is not writable)"; \
		exit 0; \
	fi
	@set -e; \
	for f in $(WII_SD_SYNC_PY_GLOB); do \
		base=$$(basename $$f); \
		if ! cp -f "$$f" "$(WII_SD_SYNC)/$$base"; then \
			echo "Note: skipping SD sync for $$base (copy failed)"; \
			exit 0; \
		fi; \
		src_sz=$$(wc -c <"$$f" | tr -d ' '); \
		dst_sz=$$(wc -c <"$(WII_SD_SYNC)/$$base" | tr -d ' '); \
		echo "SD sync: $$base ($$src_sz bytes) -> $$dst_sz bytes"; \
		if [ "$$dst_sz" = "0" ]; then \
			echo "ERROR: copied empty file to $(WII_SD_SYNC)/$$base"; \
			exit 1; \
		fi; \
	done

ifdef SD_APP_PATH

run: clean
	@$(MAKE) $(BUILD) -j$(nproc) 
	
	cp $(OUTPUT).dol $(SD_APP_PATH)/boot.dol
	sync

else

run : clean
	@$(MAKE) $(BUILD) -j$(nproc) 

	@echo "no path!"

endif

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory sync_sd
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@# Nach dem Link: Modul bauen, Symbol-Map aus dem .elf erzeugen, beides deployen.
	@$(MAKE) --no-print-directory deploy_so

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(PYMOD_SO) $(SYMBOL_MAP)

else

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)
	@test -f $(CPYTHON_BUILD)/libpython3.15.a || { \
		echo "Missing $(CPYTHON_BUILD)/libpython3.15.a (build CPython for powerpc-eabi first)."; \
		exit 1; \
	}
	$(LD) -Wl,--allow-multiple-definition $(LDFLAGS) $(CRTM) $(OFILES) $(LIBS) -o $@
	@true

pydata_%.o: %.py
	@$(bin2o)
	@mv $(<F).o $@

pngdata_%.o: %.png
	@$(bin2o)
	@mv $(<F).o $@

-include $(DEPSDIR)/*.d

endif
