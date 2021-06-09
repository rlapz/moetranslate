# MIT License
#
# moetranslate - Simple language translator written in C
#
# Copyright (c) 2021 Arthur Lapz (rLapz)
#
# See LICENSE file for license details

include config.mk

SRC = *.c
OBJ = ${SRC:.c=.o}

all: options ${TARGET}

options:
	@echo ${TARGET} build options:
	@echo "CFLAGS	= ${CFLAGS}"
	@echo "LDFLAGS	= ${LDFLAGS}"
	@echo "CC	= ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

${TARGET}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ${TARGET} ${OBJ} ${TARGET}-${VERSION}.tar.gz

dist:
	@echo creating dist tarball
	mkdir -p ${TARGET}-${VERSION}
	cp -R LICENSE Makefile config.mk ${SRC} ${TARGET}-${VERSION}
	tar -cf ${TARGET}-${VERSION}.tar ${TARGET}-${VERSION}
	gzip ${TARGET}-${VERSION}.tar
	rm -rf ${TARGET}-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${TARGET} ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${TARGET}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/${TARGET}

.PHONY: all options clean dist install uninstall
