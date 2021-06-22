# program name
TARGET = moetranslate

# program version
VERSION = ` date "+%Y%m%d" `

# paths
PREFIX = /usr
#MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. lib/*.c
LIBS = -lcurl

# flags
CFLAGS += -g -std=c99 -pedantic -Wall -Wextra ${INCS} -Os
LDFLAGS += ${LIBS}

# compiler and linker
CC ?= cc
