#include <elf.h>
#include <mod.h>

int
apply_relocate(struct secthdr *sechdrs, const char *strtab, unsigned int symindex,
               unsigned int relindex, struct module *module)
{
    struct secthdr *symsec = sechdrs + symindex;
    struct secthdr *relsec = sechdrs + relindex;
    struct secthdr *dstsec = sechdrs + relsec->sh_info;
    struct reloc_s *rel = (void *)relsec->sh_addr;
    unsigned int i;

    for (i = 0; i < relsec->sh_size / sizeof(struct reloc_s); i++, rel++) {
        unsigned long loc;
        struct symtab_s *sym;
        int offset;

        offset = GET_RELOC_SYM(rel->r_info);
        if (offset < 0 || offset > (symsec->sh_size / sizeof(struct symtab_s))) {
            kprintf("apply_relocate: %s: bad relocation, section %d reloc %d\n",
                    module->name, relindex, i);
            return -1;
        }

        sym = ((struct symtab_s *)symsec->sh_addr) + offset;

        if (rel->r_offset < 0 || rel->r_offset > dstsec->sh_size - sizeof(unsigned int)) {
            kprintf("apply_relocate: %s: out of bounds relocation, "
                "section %d reloc %d offset %d size %d\n",
                module->name, relindex, i, rel->r_offset,
                dstsec->sh_size);
            return -1;
        }

        loc = dstsec->sh_addr + rel->r_offset;

        switch (GET_RELOC_TYPE(rel->r_info)) {
        case R_ARM_NONE:
            /* ignore */
            break;

        case R_ARM_ABS32:
            *(unsigned int *)loc += sym->st_value;
            break;

        case R_ARM_PC24:
        case R_ARM_CALL:
        case R_ARM_JUMP24:
            offset = (*(unsigned int *)loc & 0x00ffffff) << 2;
            if (offset & 0x02000000)
                offset -= 0x04000000;

            offset += sym->st_value - loc;
            if (offset & 3 ||
                offset <= (int)0xfe000000 ||
                offset >= (int)0x02000000) {
                kprintf("apply_relocate: "
                    "%s: relocation out of range, section "
                    "%d reloc %d sym '%s', offset %p\n", module->name,
                    relindex, i, strtab + sym->st_name, offset);
                return -1;
            }

            offset >>= 2;

            *(int *)loc &= 0xff000000;
            *(int *)loc |= offset & 0x00ffffff;
            break;

        case R_ARM_V4BX:
            /* Preserve Rm and the condition code. Alter
            * other bits to re-code instruction as
            * MOV PC,Rm.
            */
            *(int *)loc &= 0xf000000f;
            *(int *)loc |= 0x01a0f000;
            break;

        case R_ARM_PREL31:
            offset = *(unsigned int *)loc + sym->st_value - loc;
            *(int *)loc = offset & 0x7fffffff;
            break;

        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVT_ABS:
            offset = *(int *)loc;
            offset = ((offset & 0xf0000) >> 4) | (offset & 0xfff);
            offset = (offset ^ 0x8000) - 0x8000;

            offset += sym->st_value;
            if (GET_RELOC_TYPE(rel->r_info) == R_ARM_MOVT_ABS)
                offset >>= 16;
                *(int *)loc &= 0xfff0f000;
                *(int *)loc |= ((offset & 0xf000) << 4) |
                                    (offset & 0x0fff);
            break;

        default:
            kprintf("apply_relocate: " "%s: unknown relocation: %u\n",
                module->name, GET_RELOC_TYPE(rel->r_info));
            return -1;
        }
    }
    return 0;
}

int
apply_relocate_add(struct secthdr *sechdrs, const char *strtab,
                   unsigned int symindex, unsigned int relsec, struct module *module)
{
    printk("apply_relocate_add: module %s: ADD RELOCATION unsupported\n",
               module->name);
    return -1;
}