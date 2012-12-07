.PHONY: all

SRCFILES   := ${ARCH}/sign.c
T_CC_FLAGS := -Wall -O2 -D_FILE_OFFSET_BITS=64

include ${T_BASE}/mk/comph.mk
include ${T_BASE}/mk/template.mk

all: ${T_OBJ}/sign

${T_OBJ}/sign: ${OBJFILES}
	@echo LD $@
	${V}${CC} -o$@ $^
