SRCFILES	+= $(filter %.c %.S, $(wildcard arch/${ARCH}/*))
T_CC_ALL_FLAGS	+= -m32 -Iarch/${ARCH} -DKERN_START_SECT=$(shell echo $(shell cat ${OBJPATH_ROOT}/kern-sect_size) + 1 | bc)  -D__ARCH_X86_64__

include ${T_BASE}/mk/compbl.mk
include ${T_BASE}/mk/template.mk

all: ${T_OBJ}/bootsect

${T_OBJ}/bootsect: ${T_OBJ}/bootloader ${HT_SIGN}
	@echo OBJCOPY $@
	${V}${OBJCOPY} -S -O binary $< $@.original
	@${HT_SIGN} $@.original $@

${T_OBJ}/bootloader: ${OBJFILES}
	@echo LD $@
	${V}${LD} -N -e start -Tarch/${ARCH}/bootloader.ld -o$@ ${OBJFILES}
	${V}${STRIP} -g -R .eh_frame $@
