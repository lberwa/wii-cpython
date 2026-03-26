# Stub Makefile for Grammar (modern 3.15 uses Python gen, skip for 2.0 style)
CC = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc
OPT = -g -O2 -Wall -Wstrict-prototypes -fPIC -Os -Wall -msoft-float
CFLAGS = $(OPT) -I../Include -I.. -DHAVE_CONFIG_H
AR = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ar
RANLIB = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ranlib

all: libgrammar.a

OBJS =

libgrammar.a: $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

add2lib:
	echo libgrammar.a >> ../add2lib

clean:
	rm -f *.o *.a
