export DEVKITPRO := /opt/devkitpro
export DEVKITPPC := $(DEVKITPRO)/devkitPPC

#CRT0 := $(DEVKITPPC)/powerpc-eabi/lib/crt0.o
CRTM := $(DEVKITPPC)/powerpc-eabi/lib/crtmain.o

TARGET     := hello_world
BUILD      := build
# alle Unterordner von source (inklusive source selbst)
#SOURCES := $(shell find source -type d)
SOURCES += source source/ffmpeg/libavutil/kompilieren source/ffmpeg/libavcodec/kompilieren source/mplayer/libvo source/mplayer/osdep source/mplayer/ source/ffmpeg/libavfilter/libmpcodecs/kompilieren

#source/ffmpeg source/osdep #source/ffmpeg/libavcodec
#SOURCES    += source
#SOURCES    += source/video
#SOURCES    += source/video/libavutil
#SOURCES    += source/video/osdep

DATA       :=
INCLUDES   :=
NAME	   := boot

INCLUDE    += -I/opt/devkitpro/libogc/include
INCLUDE    += -I/opt/devkitpro/extras/bitmap/include
INCLUDE    += -I/opt/devkitpro/include
INCLUDE    += -I/opt/devkitpro/extras/libmp4/include
INCLUDE    += -I/opt/devkitpro/portlibs/wii/include/
INCLUDE    += -I/opt/devkitpro/portlibs/ppc/include/
INCLUDE	   += -I/home/lew/Dokumente/wii-programme/video/source/ffmpeg


CFLAGS     += -O0 -g -std=c99 -Wall -Wextra -DGEKKO
CXXFLAGS   := $(CFLAGS)
CPPFLAGS   += -O0 -g
CFLAGS     += $(INCLUDE)
CPPFLAGS   += $(INCLUDE)

# Includes & Libs
LDFLAGS += -T/opt/devkitpro/devkitPPC/powerpc-eabi/lib/rvl.ld

LDFLAGS    += -L/opt/devkitpro/libogc/lib/wii
# Nur, wenn libbitmap.a wirklich existiert:
LDFLAGS    += -L/opt/devkitpro/extras/bitmap/lib
LDFLAGS	   += -L/opt/devkitpro/libogc/lib/wii
LDFLAGS    += -L/opt/devkitpro/extras/mysd_sdkarte_mounten/lib
LDFLAGS    += -L/opt/devkitpro/portlibs/wii/lib
LDFLAGS    += -L/opt/devkitpro/extras/libmp4/lib

LIBS       := -lwiiuse -lbte -lbitmap -lmyfat -logc -lm #-llodepng #-lmp4 -ljpeg

include $(DEVKITPPC)/wii_rules

# --------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT    := $(CURDIR)/$(NAME)
export VPATH     := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR   := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

ifeq ($(strip $(CPPFILES)),)
  export LD := $(CC)
else
  export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)

export INCLUDE += $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(BUILD)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD)/%.o: source/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: source/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)
	$(LD) $(LDFLAGS) $(CRTM) $(OFILES) $(LIBS) -o $@

-include $(DEPSDIR)/*.d

endif

