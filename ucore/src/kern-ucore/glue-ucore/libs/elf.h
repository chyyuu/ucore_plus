#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <types.h>

#ifdef __BIG_ENDIAN__
#define ELF_MAGIC   0x7F454C46U         // "\x7FELF" in big endian
#else
#define ELF_MAGIC   0x464C457FU         // "\x7FELF" in little endian
#endif

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFMAG "\177ELF"
#define SELFMAG 4

/* These constants define the different elf file types */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff


#define EM_NONE     0
#define EM_M32      1
#define EM_SPARC    2
#define EM_386      3
#define EM_68K      4
#define EM_88K      5
#define EM_486      6   /* Perhaps disused */
#define EM_860      7
#define EM_MIPS     8   /* MIPS R3000 (officially, big-endian only) */
                /* Next two are historical and binaries and
                   modules of these types will be rejected by
                   Linux.  */
#define EM_MIPS_RS3_LE  10  /* MIPS R3000 little-endian */
#define EM_MIPS_RS4_BE  10  /* MIPS R4000 big-endian */

#define EM_PARISC   15  /* HPPA */
#define EM_SPARC32PLUS  18  /* Sun's "v8plus" */
#define EM_PPC      20  /* PowerPC */
#define EM_PPC64    21   /* PowerPC64 */
#define EM_SPU      23  /* Cell BE SPU */
#define EM_SH       42  /* SuperH */
#define EM_SPARCV9  43  /* SPARC v9 64-bit */
#define EM_IA_64    50  /* HP/Intel IA-64 */
#define EM_X86_64   62  /* AMD x86-64 */
#define EM_S390     22  /* IBM S/390 */
#define EM_CRIS     76  /* Axis Communications 32-bit embedded processor */
#define EM_V850     87  /* NEC v850 */
#define EM_M32R     88  /* Renesas M32R */
#define EM_H8_300   46  /* Renesas H8/300,300H,H8S */
#define EM_MN10300  89  /* Panasonic/MEI MN10300, AM33 */
#define EM_BLACKFIN     106     /* ADI Blackfin Processor */
#define EM_TI_C6000 140 /* TI C6X DSPs */
#define EM_FRV      0x5441  /* Fujitsu FR-V */
#define EM_AVR32    0x18ad  /* Atmel AVR32 */
#define EM_ARM      40

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC   2
#define STT_SECTION 3
#define STT_FILE   4
#define STT_LOPROC 13
#define STT_HIPROC 15

#define ELF_ST_BIND(x)          ((x) >> 4)
#define ELF_ST_TYPE(x)          (((unsigned int) x) & 0xf)

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11

/* sh_flags */
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_MASKPROC    0xf0000000


#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_LOOS        0xff20
#define SHN_HIOS        0xff3f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_XINDEX      0xffff
#define SHN_HIRESERVE   0xffff


/* values for Proghdr::p_type */
#define ELF_PT_LOAD                     1

#define ELF_PT_INTERP					3

/* flag bits for Proghdr::p_flags */
#define ELF_PF_X                        1
#define ELF_PF_W                        2
#define ELF_PF_R                        4

/* values for elf's dynamic linker */
#define ELF_AT_NULL						0
#define ELF_AT_EXEFD					2
#define	ELF_AT_PHDR						3
#define ELF_AT_PHENT					4
#define ELF_AT_PHNUM					5
#define ELF_AT_BASE						7
#define ELF_AT_ENTRY					9


#ifdef __UCORE_64__
#define elf_check_arch(x) \
    ((x)->e_machine == EM_X86_64)



/* file header */
struct elfhdr {
    uint32_t e_magic;     // must equal ELF_MAGIC
    uint8_t e_elf[12];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

/* program section header */
struct proghdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_va;
    uint64_t p_pa;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct secthdr {
     uint32_t sh_name;
     uint32_t sh_type;
     uint64_t sh_flags;
     uint64_t sh_addr;
     uint64_t sh_offset;
     uint64_t sh_size;
     uint32_t sh_link;
     uint32_t sh_info;
     uint64_t sh_addralign;
     uint64_t sh_entsize;
};

struct reloc_s {
    uint64_t r_offset;         /* Location at which to apply the action */
    uint64_t r_info;           /* index and type of relocation */
};

struct reloc_a_s {
    uint64_t r_offset;         /* Location at which to apply the action */
    uint64_t r_info;           /* index and type of relocation */
    int64_t r_addend;          /* Constant addend used to compute value */
};

struct symtab_s {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};


#define GET_RELOC_SYM(i) ((i)>>32)
#define GET_RELOC_TYPE(i) ((i)&0xffffffff)
#define R_X86_64_NONE           0       /* No reloc */
#define R_X86_64_64             1       /* Direct 64 bit  */
#define R_X86_64_PC32           2       /* PC relative 32 bit signed */
#define R_X86_64_GOT32          3       /* 32 bit GOT entry */
#define R_X86_64_PLT32          4       /* 32 bit PLT address */
#define R_X86_64_COPY           5       /* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT       6       /* Create GOT entry */
#define R_X86_64_JUMP_SLOT      7       /* Create PLT entry */
#define R_X86_64_RELATIVE       8       /* Adjust by program base */
#define R_X86_64_GOTPCREL       9       /* 32 bit signed pc relative
                                           offset to GOT */
#define R_X86_64_32             10      /* Direct 32 bit zero extended */
#define R_X86_64_32S            11      /* Direct 32 bit sign extended */
#define R_X86_64_16             12      /* Direct 16 bit zero extended */
#define R_X86_64_PC16           13      /* 16 bit sign extended pc relative */
#define R_X86_64_8              14      /* Direct 8 bit sign extended  */
#define R_X86_64_PC8            15      /* 8 bit sign extended pc relative */

#define R_X86_64_NUM            16


#else /* __UCORE_64__ not defined */

#ifndef ARCH_ARM
#define elf_check_arch(x) \
	(((x)->e_machine == EM_386) || ((x)->e_machine == EM_486))
#else
#define elf_check_arch(x) \
    (((x)->e_machine == EM_ARM))
#endif

struct elfhdr {
    uint32_t e_magic;     // must equal ELF_MAGIC
    uint8_t e_elf[12];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

/* program section header */
struct proghdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_va;
    uint32_t p_pa;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct secthdr {
     uint32_t sh_name;
     uint32_t sh_type;
     uint32_t sh_flags;
     uint32_t sh_addr;
     uint32_t sh_offset;
     uint32_t sh_size;
     uint32_t sh_link;
     uint32_t sh_info;
     uint32_t sh_addralign;
     uint32_t sh_entsize;
};

struct reloc_s {
    uint32_t r_offset;         /* Location at which to apply the action */
    uint32_t r_info;           /* index and type of relocation */
};

struct reloc_a_s {
    uint32_t r_offset;         /* Location at which to apply the action */
    uint32_t r_info;           /* index and type of relocation */
    int32_t r_addend;          /* Constant addend used to compute value */
};

struct symtab_s {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    
};


#define GET_RELOC_SYM(i) ((i)>>8)
#define GET_RELOC_TYPE(i) ((i)&0xff)

#define GET_SYMTAB_BIND(i) ((i) >> 4)
#define GET_SYMTAB_TYPE(i) ((i) & 0xf)

#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC     10
#define R_386_NUM       11 
#define R_ARM_NONE              0 
#define R_ARM_PC24              1 
#define R_ARM_ABS32             2
#define R_ARM_CALL              28
#define R_ARM_JUMP24            29
#define R_ARM_V4BX              40
#define R_ARM_PREL31            42
#define R_ARM_MOVW_ABS_NC       43
#define R_ARM_MOVT_ABS          44

#endif /* __UCORE_64__ */




#endif /* !__LIBS_ELF_H__ */

