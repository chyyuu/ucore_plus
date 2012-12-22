SRCFILES	+= $(shell find arch/${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') ../glue-kern/arch/${ARCH}/bootinfo.S
ARCH_DIRS	:= debug driver process glue-ucore sync glue-ucore/libs syscall module
T_CC_FLAGS	+= ${foreach dir,${ARCH_DIRS},-Iarch/${ARCH}/${dir}}

include ${T_BASE}/mk/compk.mk
include ${T_BASE}/mk/template.mk

all: ${OBJFILES}
