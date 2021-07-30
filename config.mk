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
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wno-deprecated-declarations -Os ${INCS}
LDFLAGS += ${LIBS}

# compiler and linker
CC ?= cc
