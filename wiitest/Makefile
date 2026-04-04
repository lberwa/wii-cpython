#CRT0 := $(DEVKITPPC)/powerpc-eabi/lib/crt0.o
CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

TARGET     := hello_world
BUILD      := build
SOURCES    := source
DATA       := data
INCLUDES   :=

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

INCLUDE += -I../build-wii
INCLUDE += -I$(MAKEFILE_DIR)../Include
INCLUDE += -I$(DEVKITPRO)/libogc/include


CFLAGS     += -O0 -g -std=c99 -Wall -Wextra
CXXFLAGS   := $(CFLAGS)
CPPFLAGS   += -O0 -g
CFLAGS     += $(INCLUDE)
CPPFLAGS   += $(INCLUDE)

# Includes & Libs
LDFLAGS += -T$(DEVKITPPC)/powerpc-eabi/lib/rvl.ld

LDFLAGS    += -L$(DEVKITPRO)/libogc/lib/wii
LDFLAGS    += -L$(MAKEFILE_DIR)../libs
LDFLAGS    += -L$(MAKEFILE_DIR)..+

CPYTHON_BUILD := $(MAKEFILE_DIR)../libs


#LIBS       := -lpython3.15 -lfatpy -lwiiuse -lbte -lbitmap -logc -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -ltfpsacrypto
LIBS := -lpython3.15 -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -ltfpsacrypto -lz -lfatpy -lwiiuse -lbte -lbitmap -logc -lm


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
	@test -f $(CPYTHON_BUILD)/libpython3.15.a || { \
		echo "Missing $(CPYTHON_BUILD)/libpython3.15.a (build CPython for powerpc-eabi first)."; \
		exit 1; \
	}
	$(LD) $(LDFLAGS) $(CRTM) $(OFILES) $(LIBS) -o $@

pydata_%.o: %.py
	@$(bin2o)
	@mv $(<F).o $@

pngdata_%.o: %.png
	@$(bin2o)
	@mv $(<F).o $@

-include $(DEPSDIR)/*.d

endif
