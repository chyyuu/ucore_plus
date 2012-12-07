.PHONY: all

MOD    := supervisor

T_BASE := $(TOPDIR)
T_OBJ  := $(OBJPATH_ROOT)/supervisor

CC     := $(TARGET_CC)
LD     := $(TARGET_LD)
OBJDUMP:= $(TARGET_OBJDUMP)
OBJCOPY:= $(TARGET_OBJCOPY)
STRIP  := $(TARGET_STRIP)

V      ?= @

SRCFILES	:= $(shell find . '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' -and -not -regex '\./arch.*' | sed -e 's!\./!!g')
T_CC_FLAGS	:= -m64 -mcmodel=large -D__SUPERVISOR__ -I../glue-kern/arch/${ARCH} -Idebug -Idriver -Iinit -Ilibs -Imm -Itrap -Isync -Imp -Icontext -nostdlib -fno-builtin

$(T_OBJ):
	@mkdir $(T_OBJ)

include arch/$(ARCH)/supervisor.mk
