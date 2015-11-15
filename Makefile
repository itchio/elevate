
CFLAGS:=-std=gnu99 -Wall -Os
GCC:=gcc
STRIP:=strip

ELEVATE_CFLAGS?=-DELEVATE_VERSION=\"head\"

ifneq (${TRIPLET},)
GCC:=${TRIPLET}-${GCC}
STRIP:=${TRIPLET}-${STRIP}
endif

all:
	${GCC} ${CFLAGS} ${ELEVATE_CFLAGS} -static src/elevate.c -o elevate.exe
	${STRIP} elevate.exe
