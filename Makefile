
CFLAGS:=-std=gnu99 -Wall -Os
LDFLAGS:=-luserenv
GCC:=gcc
WINDRES:=windres
STRIP:=strip

ELEVATE_CFLAGS?=-DELEVATE_VERSION=\"head\"

ifneq (${TRIPLET},)
GCC:=${TRIPLET}-${GCC}
WINDRES:=${TRIPLET}-${WINDRES}
STRIP:=${TRIPLET}-${STRIP}
endif

all:
	${WINDRES} resources/elevate.rc -O coff -o elevate.res
	${GCC} ${CFLAGS} ${ELEVATE_CFLAGS} -static src/elevate.c elevate.res -o elevate.exe ${LDFLAGS}
	${STRIP} elevate.exe
