# Stub Makefile for Python (add2lib only)
CC = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc
OPT = -mhard-float -DTERMINAL_PRINT_DEBUG -I/opt/devkitpro/extras/bitmap/include -g -O2 -Wall -Wstrict-prototypes -fPIC -Os -Wall -msoft-float
CFLAGS = $(OPT) -I../Include -I.. -DHAVE_CONFIG_H
AR = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ar
RANLIB = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ranlib

all:

add2lib:
	echo libpython.a >> ../add2lib

clean:
