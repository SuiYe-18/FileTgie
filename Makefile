# FileTgie Linux build (gcc/clang). Usage: make
# Verified on Debian/Ubuntu-like x86_64 Linux with gcc -Wall -Wextra clean.
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS  ?= -lm
SRCS    = src/main.c src/commands.c src/port.c src/util.c src/i18n.c src/update.c
TARGET  = FileTgie

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
