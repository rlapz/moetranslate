# MIT License
#
# moetranslate - Simple language translator written in C
#
# Copyright (c) 2025 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET    = moetranslate
VERSION   = 0.9.0

PREFIX    = /usr
CC        = cc
CFLAGS    = -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L -O3
LFLAGS    = -lreadline

SRC       = moetranslate.c
OBJ       = $(SRC:.c=.o)

FILE_DIST = README.md LICENSE Makefile moetranslate.c config.def.h json.h
# ------------------------------------------------------------------- #


WNO_INTERACTIVE_MODE ?= 0

ifeq ($(WNO_INTERACTIVE_MODE), 1)
	CFLAGS += -DWNO_INTERACTIVE_MODE
	LFLAGS =
endif


all: options $(TARGET)

$(OBJ): config.h

config.h:
	cp config.def.h $(@)

moetranslate.o: $(TARGET).c
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(TARGET): $(OBJ)
	@printf "\n%s\n" "Linking: $(^)..."
	$(CC) -o $(@) $(^) $(LFLAGS)
# ------------------------------------------------------------------- #

options:
	@echo $(TARGET) build options:
	@echo "CFLAGS"  = $(CFLAGS)
	@echo "CC"      = $(CC)

clean:
	@echo cleaning
	rm -f $(OBJ) $(TARGET) moetranslate*.tar.gz

dist: clean
	@echo creating dist tarball
	mkdir -p $(TARGET)-$(VERSION)
	cp -R $(FILE_DIST) $(TARGET)-$(VERSION)
	tar -cf $(TARGET)-$(VERSION).tar $(TARGET)-$(VERSION)
	gzip $(TARGET)-$(VERSION).tar
	rm -rf $(TARGET)-$(VERSION)

install: all
	@echo installing executable file to $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(TARGET) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	@echo removing executable file from $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
# ------------------------------------------------------------------- #

.PHONY: all options clean dist install uninstall

