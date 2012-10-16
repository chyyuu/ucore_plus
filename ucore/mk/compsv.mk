CC      := i386-elf-gcc ${TARGET_CC_FLAGS_COMMON} ${TARGET_CC_FLAGS_SV}
LD		:= i386-elf-ld ${TARGET_LD_FLAGS}

include ${T_BASE}/mk/compopt.mk
