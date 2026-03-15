# Makefile – Xulebra networked snake
#
# Targets
#   all     Build the xulebra binary  (default)
#   clean   Remove object files and the binary

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lcurses

BINDIR  = bin
SRCDIR  = src

SOURCES = $(SRCDIR)/main.c        \
          $(SRCDIR)/server.c      \
          $(SRCDIR)/score.c       \
          $(SRCDIR)/one_player.c  \
          $(SRCDIR)/client.c

OBJECTS = $(SOURCES:.c=.o)
TARGET  = $(BINDIR)/xulebra

.PHONY: all clean

all: $(BINDIR) $(TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	strip $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
	rmdir --ignore-fail-on-non-empty $(BINDIR) 2>/dev/null || true
