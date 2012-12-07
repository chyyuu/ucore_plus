SRCFILES	+= ${filter %.c %.S, ${wildcard arch/${ARCH}/*/*}}
ARCH_DIRS	:= context debug driver init libs mm mp sync trap
T_CC_FLAGS	+= ${foreach dir,${ARCH_DIRS},-Iarch/${ARCH}/${dir}}

LINK_FILE	:= arch/${ARCH}/supervisor.ld

include ${T_BASE}/mk/compsv.mk
include ${T_BASE}/mk/template.mk

ifeq (${HAS_DRIVER_OS},y)
OBJFILES   += ${T_OBJ}/bzImage.o
T_CC_FLAGS += -DHAS_DRIVER_OS=1
endif

all: ${T_OBJ}/supervisor

${T_OBJ}/supervisor: ${OBJFILES}
	@echo LD $@
	${V}${LD} -T ${LINK_FILE} -z max-page-size=0x1000 -o$@ $+
