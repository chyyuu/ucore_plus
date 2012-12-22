ARCH_INLUCDES:=debug driver include libs mm module process sync trap syscall glue-ucore glue-ucore/libs $(TOPDIR)/src/glue_kern/arch/$(ARCH)
ARCH_CFLAGS := -m64 -mcmodel=kernel
ARCH_LDFLAGS := -melf_x86_64
