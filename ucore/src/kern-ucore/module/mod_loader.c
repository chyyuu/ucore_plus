#include "mod.h"
#include <stdio.h>
#include <string.h>
#include <kio.h>
#include <file.h>
#include <stat.h>
#include <slab.h>
#include <elf.h>
#include <bitsearch.h>
#include <mmu.h>
#include <mod_loader.h>

#define CHARS_MAX            100000
static uint32_t char_count;
static char     chars[CHARS_MAX];

#define EXPORT_SYM_COUNT_MAX 1024
#define EXPORT_SYM_HASH      197

static uint32_t    ex_sym_count;

static int         ex_sym_f[EXPORT_SYM_HASH];
static const char *ex_sym_name[EXPORT_SYM_COUNT_MAX];
static uintptr_t   ex_sym_ptr[EXPORT_SYM_COUNT_MAX];
static uint32_t    ex_sym_flags[EXPORT_SYM_COUNT_MAX];
static int         ex_sym_n[EXPORT_SYM_COUNT_MAX];

static uint32_t sym_hash(const char *name, uint32_t len);
static int elf_head_check(void * elf);
static int elf_mod_parse(uintptr_t elf, const char *name, int export_symbol, uintptr_t * common_data, uint32_t * common_size, uintptr_t * mod_load_ptr, uintptr_t * mod_unload_ptr);

extern uintptr_t bss_pool_ptr;

// initialize export symbol table and counter
void mod_loader_init() {
    ex_sym_count = 0;
    char_count = 0;
    
    memset(ex_sym_f, -1, sizeof(ex_sym_f));
}


// get symbol pointer using index
uintptr_t get_sym_ptr(int idx) {
    if (idx < 0 || idx >= ex_sym_count) {
        return -1;
    }
    return ex_sym_ptr[idx];
}

// load module into kernel
int elf_mod_load(uintptr_t image, uint32_t image_size, struct elf_mod_info_s * info) {
    info->image = image;
    info->image_size = image_size;

    if (elf_head_check((void *)image)) {
        return -1;
    }
    
    if (elf_mod_parse(image, "", 0, &info->common_ptr, &info->common_size, &info->load_ptr, &info->unload_ptr)) {
        return -1;
    }

    return 0;
}

// get symbol string for elf image using index
static const char * get_symbol_string(uintptr_t elf, uint32_t index) {
    uint32_t i;
    struct elfhdr * eh;
    struct secthdr * sh;
    //struct symtab_s * symtab;

    eh = (struct elfhdr *)elf;

    for (i = 0; i < eh->e_shnum; i++) {
        sh = (struct secthdr *)(elf + eh->e_shoff + (i * eh->e_shentsize));
        if (sh->sh_type == SH_TYPE_SYMTAB) {
            //symtab = (struct symtab_s *)(elf + sh->sh_offset);
            sh = (struct secthdr *)(elf + eh->e_shoff + (sh->sh_link * eh->e_shentsize));
            if (sh->sh_type == SH_TYPE_STRTAB) {
                if (index == 0) {
                    return " ";
                } else {
                    return (char *)(elf + sh->sh_offset + index);
                }
            }
        }
    }
    return "";
}


static uintptr_t get_section_offset(uintptr_t elf, uint32_t info) {
    struct elfhdr * eh;
    struct secthdr * sh;
    eh = (struct elfhdr *)elf;
    sh = (struct secthdr *)(elf + eh->e_shoff + (info * eh->e_shentsize));

    uintptr_t result;
    if (sh->sh_type == SH_TYPE_NOBITS) {
        result = sh->sh_addr;
    } else {
        result = sh->sh_offset;
    }
    return result;
}

int find_export_sym(const char *name, int touch) {
    int name_len = strlen(name);
    int h = sym_hash(name, name_len);
    int cur = ex_sym_f[h];

    while (cur != -1) {
        if (strcmp(ex_sym_name[cur], name) == 0) {
            break;
        } else {
            cur = ex_sym_n[cur];
        }
    }

    if (cur == -1 && touch) {
        cur = ex_sym_count++;
        ex_sym_n[cur] = ex_sym_f[h];
        ex_sym_f[h] = cur;

        char * _name = chars + char_count;
        memmove(_name, name, name_len);
        char_count += name_len;
        chars[char_count++] = 0;
        ex_sym_name[cur] = _name;
    }
    return cur;
}

void mod_touch_symbol(const char *name, void * ptr, uint32_t flags) {
    int idx = find_export_sym(name, 1);
    ex_sym_ptr[idx] = (uintptr_t)ptr;
    ex_sym_flags[idx] = flags;
}

void mod_disable_symbol(const char *name) {
    int idx = find_export_sym(name, 0);
    if (idx >= 0) {
        ex_sym_ptr[idx] = 0;
    }
}

static int elf_mod_create_symbol(const char *name, void *ptr, uint32_t flags) {
    int idx = find_export_sym(name, 1);
    if (idx != ex_sym_count - 1) {
        return -1;
    }
    ex_sym_ptr[idx] = (uintptr_t)ptr;
    ex_sym_flags[idx] = flags;
    return 0;
}

static int elf_mod_get_symbol(const char *name, void **ptr, uint32_t *flags) {
    int idx = find_export_sym(name, 0);
    if (idx == -1) {
        return -1;
    }
    if (ptr != NULL) {
        *ptr = (void *)ex_sym_ptr[idx];
    }
    if (flags != NULL) {
        *flags = ex_sym_flags[idx];
    }
    return 0;
}

static struct symtab_s * fill_symbol_struct(uintptr_t elf, uint32_t symbol) {
    uint32_t i;
    struct elfhdr *eh;
    struct secthdr * sh;
    struct symtab_s * symtab;

    eh = (struct elfhdr *)elf;
    for (i = 0; i < eh->e_shnum; i++) {
        sh = (struct secthdr *)(elf + eh->e_shoff + (i * eh->e_shentsize));
        if (sh->sh_type == SH_TYPE_SYMTAB) {
            symtab = (struct symtab_s*)(elf + sh->sh_offset + (symbol * sh->sh_entsize));
            return (struct symtab_s *)symtab;
        }
    }
    return (struct symtab_s *)0;
}

static int elf_mod_parse(uintptr_t elf, const char *name, int export_symbol, 
        uintptr_t * common_data, uint32_t * common_size,
        uintptr_t * mod_load_ptr, uintptr_t * mod_unload_ptr) {
    uint32_t i, x;
    uintptr_t reloc_addr;
    void * mem_addr;

    struct elfhdr *eh;
    struct secthdr *sh;
    struct reloc_a_s *reloc;
    struct symtab_s *symtab;

    uintptr_t cur_common_alloc = 0;
    uintptr_t cur_common_align = 1;

    eh = (struct elfhdr *)elf;

    info("shnum = %d, shsize = %d\n", eh->e_shnum, eh->e_shentsize);

    for (i = 0; i < eh->e_shnum; i++) {
        sh = (struct secthdr*)(elf + eh->e_shoff + (i * eh->e_shentsize));
        info("sh type = %d\n", sh->sh_type);
        if (sh->sh_type == SH_TYPE_SYMTAB) {
            for (x = 0; x < sh->sh_size; x += sh->sh_entsize) {
                symtab = (struct symtab_s *)(elf + sh->sh_offset + x);
                info("found sym: [%s] info[%02x] size[%d] addr[%08x]\n",
                        get_symbol_string(elf, symtab->sym_name),
                        symtab->sym_info,
                        symtab->sym_size,
                        symtab->sym_address);
                if (symtab->sym_shndx != SHN_UNDEF && symtab->sym_shndx < 0xff00) {
                    info("    value offset [%08x]\n", get_section_offset(elf, symtab->sym_shndx) + symtab->sym_address);
                    const char * sym_name = get_symbol_string(elf, symtab->sym_name);
                    switch (GET_SYMTAB_BIND(symtab->sym_info)) {
                        case STB_GLOBAL:
                            info("    global symbol\n");
                        case STB_LOCAL:
                            if (strcmp(sym_name, MOD_INIT_MODULE) == 0) {
                                *mod_load_ptr = get_section_offset(elf, symtab->sym_shndx) + symtab->sym_address + elf;
                            } else if (strcmp(sym_name, MOD_CLEANUP_MODULE) == 0) {
                                *mod_unload_ptr = get_section_offset(elf, symtab->sym_shndx) + symtab->sym_address + elf;
                            }

                            // global
                            if (GET_SYMTAB_BIND(symtab->sym_info) == 1 && export_symbol) {
                                mod_touch_symbol(sym_name, (void *)(get_section_offset(elf, symtab->sym_shndx) + symtab->sym_address + elf), 0);
                            }
                            break;

                        case STB_WEAK:
                            info("    weak symbol\n");
                            if (export_symbol) {
                                elf_mod_create_symbol(sym_name, (void *)(symtab->sym_address + elf), 0);
                            }
                            break;
                    }
                } else if (symtab->sym_shndx == SHN_COMMON) {
                    // TODO: implement SHN_COMMON
                    info("SHN_COMMON, alloc %d byte offset %d\n", symtab->sym_size, cur_common_alloc);
                    symtab->sym_address = cur_common_alloc;
                    cur_common_alloc += symtab->sym_size;
                } else {
                    info("shndx[%04x]\n", symtab->sym_shndx);
                }
            }
        } else if (sh->sh_type == SH_TYPE_NOBITS) {
            info("bss section, alloc %d byte align 0x%x\n", sh->sh_size, sh->sh_addralign);
            if (bsf(sh->sh_addralign != bsr(sh->sh_addralign))) {
                error(" bad align\n");
                return -1;
            }
            if (sh->sh_addralign > cur_common_align) {
                cur_common_align = sh->sh_addralign;
            }
            cur_common_alloc = ((cur_common_alloc - 1) | (sh->sh_addralign - 1)) + 1;
            sh->sh_addr = cur_common_alloc;
            cur_common_alloc += sh->sh_size;
        }
    }

    uintptr_t common_space;
    if (cur_common_align > PGSIZE) {
        error("align failed\n");
        return -1;
    } else if (cur_common_alloc > 0) {
        // TODO: kmalloc would return non-kernel-area memory pointer, which would cause PG FAULT
        // common_space = (uintptr_t)kmalloc(cur_common_alloc);
        
        common_space = bss_pool_ptr;
        if ((void *)common_space == NULL) {
            error("no enough memory for bss\n");
            return -1;
        }
        bss_pool_ptr += cur_common_alloc;

        error("memory pointer: %016llx\n", common_space);

        memset((void *)common_space, 0, cur_common_alloc);

        *common_data = common_space;
        *common_size = cur_common_alloc;
    } else {
        *common_data = 0;
        *common_size = 0;
    }

    // fill the relocation entries
    for (i = 0; i < eh->e_shnum; i++) {
        sh = (struct secthdr *)(elf + eh->e_shoff + (i * eh->e_shentsize));

        if (sh->sh_type == SH_TYPE_RELA) {
            for (x = 0; x < sh->sh_size; x += sh->sh_entsize) {
                reloc = (struct reloc_a_s *)(elf + sh->sh_offset + x);
                symtab = fill_symbol_struct(elf, GET_RELOC_SYM(reloc->rl_info));

                info("reloc[%02x] offset[%08x] for [%s], sym offset[%08x]\n",
                        GET_RELOC_TYPE(reloc->rl_info),
                        reloc->rl_offset,
                        get_symbol_string(elf, symtab->sym_name),
                        symtab->sym_address);

                mem_addr = (void *)(elf + reloc->rl_offset + get_section_offset(elf, sh->sh_info));

                /* external reference (kernel symbol most likely) */
                if (symtab->sym_shndx == SHN_UNDEF) {
                    const char *sym_name = get_symbol_string(elf, symtab->sym_name);
                    int idx = find_export_sym(sym_name, 0);
                    if (idx == -1) {
                        if (strcmp(sym_name, name) == 0) {
                            reloc_addr = elf;
                        } else {
                            error("unresolved symbol \"%s\", set with 0\n", sym_name);
                            reloc_addr = 0;
                        }
                    } else {
                        info("external symbol %s addr = %p\n", sym_name, ex_sym_ptr[idx]);
                        reloc_addr = ex_sym_ptr[idx];
                    }
                } else if (symtab->sym_shndx < 0xff00) {
                    info("section offset %16x, addr %16x\n", get_section_offset(elf, symtab->sym_shndx), symtab->sym_address);
                    if (((struct secthdr *)(elf + eh->e_shoff + (symtab->sym_shndx * eh->e_shentsize)))->sh_type == SH_TYPE_NOBITS) {
                        reloc_addr = common_space;
                    } else {
                        reloc_addr = elf;
                    }
                    reloc_addr += get_section_offset(elf, symtab->sym_shndx);
                    reloc_addr += symtab->sym_address;
                } else if (symtab->sym_shndx == SHN_COMMON) {
                    reloc_addr = common_space + symtab->sym_address;
                } else {
                    error("unhandled syn_shndx\n");
                }
                
                uint64_t val = reloc_addr + reloc->rl_addend;

                switch (GET_RELOC_TYPE(reloc->rl_info)) {
                    case R_X86_64_NONE:  
                        break;
                    case R_X86_64_64:    
                        *(uint64_t *)mem_addr = val;
                        break;
                    case R_X86_64_32:
                        *(uint32_t *)mem_addr = val;
                        //if (val != *(uint32_t *)mem_addr)
                        //    error("overflow in relocation type %d. val %lld, memaddr %d.\n", GET_RELOC_TYPE(reloc->rl_info), val, *(uint32_t *)mem_addr);
                        break;
                    case R_X86_64_32S: //11
                        *(int32_t *)mem_addr = val;
                        //if ((int64_t)val != *(int32_t *)mem_addr)
                        //    error("overflow in relocation type %d. val %lld, memaddr %d.\n", GET_RELOC_TYPE(reloc->rl_info), (int64_t)val, *(int32_t *)mem_addr);
                        break;
                    case R_X86_64_PC32: //2
                        val -= (uint64_t)mem_addr;
                        *(uint32_t *)mem_addr = val;
                        //if ((int64_t)val != *(int32_t *)mem_addr)
                        //    error("overflow in relocation type %d. val %lld, memaddr %d.\n", GET_RELOC_TYPE(reloc->rl_info), (int64_t)val, *(int32_t *)mem_addr);
                        break;
                    default:
                        error("unsupported relocation type (%x)\n", GET_RELOC_TYPE(reloc->rl_info));
                        break;
                }
                info("fill rel address %08x to %08x\n", *(uint32_t *)mem_addr, mem_addr);
            }
        } else if (sh->sh_type == SH_TYPE_REL) {
            error("relocation SH_TYPE_REL not implemented\n");
        }
    }

    return 0;
}

static int elf_head_check(void * elf) {
    info("begin elf header check\n");
    struct elfhdr * eh = (struct elfhdr *)elf;
    if (eh->e_magic != ELF_MAGIC) {
        error("invalid signature: %x\n", eh->e_magic);
        return -1;
    }

    if (eh->e_elf[0] != 0x2) {
        error("not ELFCLASS64");
        return -1;
    }

    if (eh->e_type != 0x01) {       // ET_REL , Relocatable object file
        error("error type: %d\n", eh->e_type);
        return -1;
    }

    if (eh->e_machine != 0x3e) {    // AMD x86-64 architecture
        error("error machine type: %d \n", eh->e_machine);
        return -1;
    }

    if (eh->e_entry != 0x0) {
        error("error entry code: %d\n", eh->e_entry);
        return -1;
    }
    
    info("elf head check passed !\n");
    return 0;
}

void touch_export_sym(const char *name, uintptr_t ptr, uint32_t flags) {
    int name_len = strlen(name);
    int h = sym_hash(name, name_len);
    int cur = ex_sym_f[h];

    while (cur != -1) {
        if (strcmp(ex_sym_name[cur], name) == 0) {
            // symbol found
            break;
        } else {
            cur = ex_sym_n[cur];
        }
    }
    // not found, create one
    if (cur == -1) {
        cur = ex_sym_count++;
        ex_sym_n[cur] = ex_sym_f[h];
        ex_sym_f[h] = cur;

        char * _name = chars + char_count;
        memmove(_name, name, name_len);
        char_count += name_len;
        ex_sym_name[cur] = _name;
        chars[char_count++] = 0;
    }

    ex_sym_ptr[cur] = ptr;
    ex_sym_flags[cur] = flags;
}

static uint32_t sym_hash(const char *name, uint32_t len) {
    int i;
    uint32_t ret = 0;
    for (i = 0; i != len; i++) {
        ret = (ret * 13 + name[i]) % EXPORT_SYM_HASH;
    }
    return ret;
}
