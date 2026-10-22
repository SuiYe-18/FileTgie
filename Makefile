# FileTgie Linux build (gcc/clang). Usage: make
# Note: structure-only; not verified on a real Linux machine in this release.
CC      ?= gcc
CFLAGS  ?= -O2 -Wall
LDLIBS  ?= -lm
SRCS    = src/main.c src/commands.c src/port.c src/util.c src/i18n.c
TARGET  = FileTgie

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
