# Stub Makefile for Python (add2lib only)
DEVKITPRO ?= /opt/devkitpro
CC = $(DEVKITPRO)/devkitPPC/bin/powerpc-eabi-gcc
OPT = -mhard-float  -I$(DEVKITPRO)/extras/bitmap/include -g -O0 -Wall -Wstrict-prototypes -fPIC -Os -Wall -msoft-float #-DTERMINAL_PRINT_DEBUG
CFLAGS = $(OPT) -I../Include -I.. -DHAVE_CONFIG_H
AR = $(DEVKITPRO)/devkitPPC/bin/powerpc-eabi-ar
RANLIB = $(DEVKITPRO)/devkitPPC/bin/powerpc-eabi-ranlib
# 0000oooo
all:

add2lib:
	echo libpython.a >> ../add2lib

clean:
