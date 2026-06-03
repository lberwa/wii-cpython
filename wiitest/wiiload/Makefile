export DEVKITPRO := /opt/devkitpro
export DEVKITPPC := $(DEVKITPRO)/devkitPPC

TARGET := hello_world
BUILD  := build
SRC    := source
DATA   := SD

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

include $(DEVKITPPC)/wii_rules

# Startup code (required for libogc)
CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

# -----------------------------
# FILE DISCOVERY
# -----------------------------
# Note: bin2s skips empty files (no symbols) -> exclude them.
# Also exclude whitespace names (we error out early; this prevents broken deps).
FILES := $(shell find $(DATA) -type f -size +0c ! -name '* *' 2>/dev/null)

OFILES := $(foreach f,$(FILES),$(BUILD)/$(f).o)
SRC_OFILES := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c))

CFLAGS += -O2 -Wall -I$(DEVKITPRO)/libogc/include

LDFLAGS += -T$(DEVKITPPC)/powerpc-eabi/lib/rvl.ld
LDFLAGS += -L$(DEVKITPRO)/libogc/lib/wii
LDFLAGS += -L../../libs

LD := $(CC)

LIBS := -lfatpy -lwiiuse -lbte -logc -lm -lbitmap

# -----------------------------
# BUILD ORDER
# -----------------------------
all: check_data_names $(BUILD) $(TARGET).dol

$(BUILD):
	mkdir -p $(BUILD)

# Fail fast on filenames that GNU make can't represent safely in a whitespace-separated list.
.PHONY: check_data_names
check_data_names:
	@bad="$$(find $(DATA) -type f -print 2>/dev/null | LC_ALL=C grep -n '[[:space:]]' || true)"; \
	if [ -n "$$bad" ]; then \
		echo "ERROR: SD/ contains files with whitespace in the name (make can't handle this here)."; \
		echo "$$bad" | sed 's/^[0-9]*://'; \
		echo "Rename those files (no spaces/tabs/newlines) and rerun make."; \
		exit 1; \
	fi

# -----------------------------
# CODEGEN (always runs, but c_builder.py only touches main.c if content changes)
# -----------------------------
.PHONY: FORCE
FORCE:

$(SRC)/main.c: FORCE c_builder.py
	python3 c_builder.py

# -----------------------------
# ELF / DOL
# -----------------------------
$(TARGET).elf: $(SRC_OFILES) $(foreach f,$(FILES),$(BUILD)/$(f).o)
	$(LD) $(LDFLAGS) $(CRTM) $^ $(LIBS) -o $@

$(TARGET).dol: $(TARGET).elf
	elf2dol $< $@

# -----------------------------
# C FILES
# -----------------------------
$(BUILD)/%.o: $(SRC)/%.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# DATA -> bin2o (recursive)
# -----------------------------
$(BUILD)/$(DATA)/%.o: $(DATA)/%
	mkdir -p $(dir $@)
	@set -e; \
	tmpdir="$$(mktemp -d)"; \
	sym="$$(printf 'sd_%s' "$*" | tr '/.' '_' | sed 's/[^A-Za-z0-9_]/_/g')"; \
	in="$${tmpdir}/$${sym}"; \
	cp "$<" "$$in"; \
	bin2s -a 32 -H "$$tmpdir/$${sym}.h" "$$in" > "$$tmpdir/$${sym}.s"; \
	$(CC) -x assembler-with-cpp $(CPPFLAGS) $(ASFLAGS) -c "$$tmpdir/$${sym}.s" -o "$$tmpdir/$${sym}.o"; \
	mv "$$tmpdir/$${sym}.o" "$@"; \
	mv "$$tmpdir/$${sym}.h" "$(dir $@)"; \
	rm -rf "$$tmpdir"

clean:
	rm -rf $(BUILD) *.elf *.dol
