# Simple SD/USB filesystem probe (no Python) for Dolphin/Wii.

CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

TARGET     := sd_probe
BUILD      := build
SOURCES    := source
DATA       :=
INCLUDES   :=

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

INCLUDE += -I$(DEVKITPRO)/libogc/include
INCLUDE += -I$(MAKEFILE_DIR)../../../bitmap/include
INCLUDE += -I$(MAKEFILE_DIR)../../../fat/include

CFLAGS     += -O0 -g -std=c99 -Wall -Wextra
CPPFLAGS   += -O0 -g
CFLAGS     += $(INCLUDE)
CPPFLAGS   += $(INCLUDE)

LDFLAGS += -T$(DEVKITPPC)/powerpc-eabi/lib/rvl.ld
LDFLAGS += -L$(DEVKITPRO)/libogc/lib/wii
LDFLAGS += -L$(MAKEFILE_DIR)../../../libs

LIBS := -lfatpy -logc -lm

include $(DEVKITPPC)/wii_rules

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT    := $(CURDIR)/$(TARGET)
export VPATH     := $(foreach dir,$(SOURCES) $(DATA),$(CURDIR)/$(dir))
export DEPSDIR   := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    :=
sFILES      :=
SFILES      :=

export LD := $(CC)
export OFILES := $(CFILES:.c=.o)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)
	$(LD) $(LDFLAGS) $(CRTM) $(OFILES) $(LIBS) -o $@

-include $(DEPSDIR)/*.d

endif

