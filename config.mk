# program name
TARGET = moetranslate

# program version
VERSION = ` date "+%Y%m%d" `

# paths
PREFIX = /usr
#MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS =  
LIBS = -lcurl

# flags
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS += ${LIBS}

# compiler and linker
CC ?= cc
