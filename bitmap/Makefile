# Name der Bibliothek (libbitmap.a)
LIBNAME := bitmap

# Verzeichnisse
SRCDIR := source
INCDIR := include
LIBDIR := lib
OBJDIR := build

# Tools
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
AR := $(DEVKITPPC)/bin/powerpc-eabi-ar

# Compiler-Flags
INCLUDE += -I$(DEVKITPRO)/libogc/include

CFLAGS += -g -O3 -Wall -c -I$(INCDIR)
CFLAGS += $(INCLUDE)
CPPFLAGS += $(INCLUDE)

# Finde alle .c-Dateien im source/-Verzeichnis
SRC := $(wildcard $(SRCDIR)/*.c)

# Ersetze .c mit .o und speichere sie in build/
OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))

# Ziel: lib/libmylib.a
$(LIBDIR)/lib$(LIBNAME).a: $(OBJ)
	@mkdir -p $(LIBDIR)
	$(AR) rcs $@ $^

# Kompiliere jede .c zu .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $< -o $@

# Standardziel
all: $(LIBDIR)/lib$(LIBNAME).a
	@echo "Fertig: $(LIBDIR)/lib$(LIBNAME).a"

# Aufräumen
clean:
	rm -rf $(OBJDIR) $(LIBDIR)/lib$(LIBNAME).a

