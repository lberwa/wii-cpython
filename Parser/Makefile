# Simple 2.0-style Makefile for Parser (minimal for cross-build)
CC = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc
OPT = -g -O2 -Wall -Wstrict-prototypes -fPIC -Os -Wall -msoft-float
CFLAGS = $(OPT) -I../Include -I.. -DHAVE_CONFIG_H
AR = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ar
RANLIB = /opt/devkitpro/devkitPPC/bin/powerpc-eabi-ranlib

all: libparser.a

OBJS = token.o myreadline.o parser.o peg_api.o pegen.o pegen_errors.o string_parser.o

lexer/grammar.c lexer/tokenizer.c:
	echo "Stub lexer/grammar - gen skipped"
	touch lexer/grammar.c lexer/tokenizer.c

libparser.a: $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

add2lib:
	echo libparser.a >> ../add2lib

clean:
	rm -f *.o *.a lexer/*.o
