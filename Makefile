# MIT License
#
# moetranslate - Simple language translator written in C
#
# Copyright (c) 2021 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

TARGET    = moetranslate
VERSION   = 0.3.0

PREFIX    = /usr
CC        = cc
CFLAGS    = -g -std=c99 -Wall -Wextra -pedantic -fpie -fPIE -fno-omit-frame-pointer \
	    -D_POSIX_C_SOURCE=200809L -Os

SRC       = moetranslate.c lib/cJSON.c lib/util.c lib/linenoise.c
OBJ       = $(SRC:.c=.o)

FILE_DIST = README.md LICENSE Makefile moetranslate.c config.def.h lib/
# ------------------------------------------------------------------- #

all: options $(TARGET)

$(OBJ): config.h

config.h:
	cp config.def.h $(@)

moetranslate.o: $(TARGET).c
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

linenoise.o: linenoise.c linenoise.h
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

cJSON.o: cJSON.c cJSON.h
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

util.o: util.c util.h
	@printf "\n%s\n" "Compiling: $(<)..."
	$(CC) $(CFLAGS) -c -o $(@) $(<)

$(TARGET): $(OBJ)
	@printf "\n%s\n" "Linking: $(^)..."
	$(CC) -o $(@) $(^)
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

