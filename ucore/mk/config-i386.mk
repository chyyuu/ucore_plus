QEMU            		?= qemu
export HOST_CC_PREFIX	?=
export TARGET_CC_PREFIX	?= 
export TARGET_CC_FLAGS_COMMON	?=	-m32 -ffreestanding \
									-mno-red-zone \
									-mno-mmx -mno-sse -mno-sse2 \
									-mno-sse3 -mno-3dnow \
									-fno-builtin -fno-builtin-function -nostdinc
export TARGET_CC_FLAGS_BL		?=
export TARGET_CC_FLAGS_KERNEL	?=
export TARGET_CC_FLAGS_SV		?=
export TARGET_CC_FLAGS_USER		?=
export TARGET_LD_FLAGS			?=	-m $(shell ld -V | grep elf_i386 2>/dev/null) -nostdlib

qemu: all
	${QEMU} -m 512 \
	-hda ${T_OBJ}/kernel.img \
	-drive file=${T_OBJ}/swap.img,media=disk,cache=writeback \
	-drive file=${T_OBJ}/sfs.img,media=disk,cache=writeback \
	-s -S \
	-serial file:${T_OBJ}/serial.log -monitor stdio

debug: all
	${V}gdb -q -x gdbinit.${ARCH}
