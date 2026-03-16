# Makefile – Xulebra v0.2
#
# Targets
#   all     Build bin/xulebra  (default)
#   clean   Remove generated files

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2 -D_DEFAULT_SOURCE -Iinclude
LDFLAGS = -lcurses

SRCDIR  = src
INCDIR  = include
OBJDIR  = obj
BINDIR  = bin

SOURCES = $(SRCDIR)/main.c    \
          $(SRCDIR)/server.c  \
          $(SRCDIR)/client.c  \
          $(SRCDIR)/single.c  \
          $(SRCDIR)/score.c   \
          $(SRCDIR)/snake.c   \
          $(SRCDIR)/colors.c  \
          $(SRCDIR)/socket.c

OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET  = $(BINDIR)/xulebra

.PHONY: all clean

all: $(BINDIR) $(OBJDIR) $(TARGET)

$(BINDIR) $(OBJDIR):
	mkdir -p $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
	rmdir --ignore-fail-on-non-empty $(OBJDIR) $(BINDIR) 2>/dev/null || true
