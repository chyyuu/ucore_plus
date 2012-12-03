TARGET  := mod-add
WARN    := -W -Wall -Wstrict-prototypes -Wmissing-prototypes
INCLUDE := -I../../ -I../../../arch/i386/driver -I../../../arch/i386/glue-ucore/libs -I../../../glue-ucore -I../../../glue-ucore/libs -I../../../../glue-kern/arch/i386
CFLAGS  := -O2 -c ${WARN} ${INCLUDE} -fno-common -mlong-calls
CC      := arm-eabi-gcc
	
${TARGET}.ko: ${TARGET}.c
	${CC} ${CFLAGS} ${TARGET}.c -o ${TARGET}.ko

.PHONY: clean

clean:
	rm -rf ${TARGET}.ko