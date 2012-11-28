SRCFILES	+= $(filter %.c %.S, $(wildcard arch/${ARCH}/*))
T_CC_ALL_FLAGS	+= -Iarch/${ARCH} -D__ARCH_I386__ -m32

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
