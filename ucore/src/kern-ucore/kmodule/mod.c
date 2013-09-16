#include <assert.h>
#include <string.h>
#include <kio.h>
#include <file.h>
#include <stat.h>
#include <slab.h>
#include <elf.h>
#include <mmu.h>
#include <vmm.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <ide.h>
#include <moduleloader.h>
#include <mod.h>
#include <sem.h>
#include <stdlib.h>
#include <error.h>

#ifndef ARCH_SHF_SMALL
#define ARCH_SHF_SMALL 0
#endif

#define BITS_PER_LONG 32
#define INIT_OFFSET_MASK (1UL << (BITS_PER_LONG-1))

static unsigned long module_addr_min = -1UL, module_addr_max = 0;

static list_entry_t modules = { &(modules), &(modules) };

static char last_unloaded_module[MODULE_NAME_LEN + 1];

void *module_alloc(unsigned long size)
{
	return size == 0 ? NULL : kmalloc(size);
}

void module_free(struct module *module, void *region)
{
	kfree(region);
}

/*static inline void *percpu_modalloc(unsigned long size, unsigned long align,
				    const char *name)
{
	return NULL;
}*/

/*static inline unsigned int find_pcpusec(struct elfhdr *hdr,
					struct secthdr *sechdrs,
					const char *secstrings)
{
	return 0;
}*/

/*static inline int check_version(struct secthdr *sechdrs,
				unsigned int versindex,
				const char *symname,
				struct module *mod, const unsigned long *crc)
{
	return 1;
}*/

/* Resolve a symbol for this module.  I.e. if we find one, record usage. Must be holding module_mutex. */
static const struct kernel_symbol *resolve_symbol(struct secthdr *sechdrs,
						  unsigned int versindex,
						  const char *name,
						  struct module *mod)
{
	struct module *owner;
	const struct kernel_symbol *sym;
	const unsigned long *crc;
	sym = find_symbol(name, &owner, &crc, 1);
	if (sym) {
		kprintf("\tresolve_symbol: symbol %s found\n", name);
		if (/*!check_version(sechdrs, versindex, name, mod, crc) ||*/
		    !use_module(mod, owner))
			sym = NULL;
	}
	return sym;
}

static inline int same_magic(const char *amagic, const char *bmagic,
			     bool has_crcs)
{
	return strcmp(amagic, bmagic) == 0;
}

/*static inline int check_modstruct_version(struct secthdr *sechdrs,
					  unsigned int versindex,
					  struct module *mod)
{
	return 1;
}*/

static void set_license(struct module *mod, const char *license)
{
	if (!license)
		license = "unspecified";
	// TODO: the feature is left for future use.
}

static char *next_string(char *string, unsigned long *secsize)
{
	/* Skip non-zero chars */
	while (string[0]) {
		string++;
		if ((*secsize)-- <= 1)
			return NULL;
	}
	/* Skip any zero padding. */
	while (!string[0]) {
		string++;
		if ((*secsize)-- <= 1)
			return NULL;
	}
	return string;
}

static char *get_modinfo(struct secthdr *sechdrs,
			 unsigned int info, const char *tag)
{
	char *p;
	unsigned int taglen = strlen(tag);
	unsigned long size = sechdrs[info].sh_size;

	for (p = (char *)sechdrs[info].sh_addr; p; p = next_string(p, &size)) {
		if (strncmp(p, tag, taglen) == 0 && p[taglen] == '=')
			return p + taglen + 1;
	}
	return NULL;
}

extern const struct kernel_symbol __start___ksymtab[];
extern const struct kernel_symbol __stop___ksymtab[];
//extern const unsigned long __start___kcrctab[];

#ifndef CONFIG_MODVERSIONS
#define symversion(base, idx) NULL
#else
#define symversion(base, idx) ((base != NULL) ? ((base) + (idx)) : NULL)
#endif

static unsigned int find_sec(struct elfhdr *hdr,
			     struct secthdr *sechdrs,
			     const char *secstrings, const char *name)
{
	unsigned int i;
	for (i = 1; i < hdr->e_shnum; i++)
		if ((sechdrs[i].sh_flags & SHF_ALLOC)
		    && strcmp(secstrings + sechdrs[i].sh_name, name) == 0)
			return i;
	return 0;
}

static void *section_addr(struct elfhdr *hdr, struct secthdr *shdrs,
			  const char *secstrings, const char *name)
{
	/* Section 0 has sh_addr 0. */

	return (void *)shdrs[find_sec(hdr, shdrs, secstrings, name)].sh_addr;
}

static void *section_objs(struct elfhdr *hdr,
			  struct secthdr *sechdrs,
			  const char *secstrings,
			  const char *name,
			  size_t object_size, unsigned int *num)
{
	unsigned int sec = find_sec(hdr, sechdrs, secstrings, name);
	/* Section 0 has sh_addr 0 and sh_size 0. */
	*num = sechdrs[sec].sh_size / object_size;
	return (void *)sechdrs[sec].sh_addr;
}

static inline int within(unsigned long addr, void *start, unsigned long size)
{
	return ((void *)addr >= start && (void *)addr < start + size);
}

static const struct kernel_symbol *lookup_symbol(const char *name, const struct kernel_symbol
						 *start, const struct kernel_symbol
						 *stop)
{
	const struct kernel_symbol *ks = start;
	for (; ks < stop; ++ks) {
		if (strcmp(ks->name, name) == 0)
			return ks;
	}
	return NULL;
}

/*static int is_exported(const char *name, unsigned long value,
		       const struct module *mod)
{
	const struct kernel_symbol *ks;
	if (!mod)
		ks = lookup_symbol(name, __start___ksymtab, __stop___ksymtab);
	else
		ks = lookup_symbol(name, mod->syms, mod->syms + mod->num_syms);
	return ks != NULL && ks->value == value;
}

// get the module whose code contains an address
struct module *__module_text_address(unsigned long addr)
{
	struct module *mod = __module_address(addr);
	if (mod) {
		if (!within(addr, mod->module_init, mod->init_text_size)
		    && !within(addr, mod->module_core, mod->core_text_size))
			mod = NULL;
	}
	return NULL;
}

EXPORT_SYMBOL(__module_text_address);

// get the module which contains an address
struct module *__module_address(unsigned long addr)
{
	struct module *mod;

	if (addr < module_addr_min || addr > module_addr_max)
		return NULL;

	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		if (within_module_core(addr, mod)
		    || within_module_init(addr, mod))
			return mod;
	}
	return NULL;
}

EXPORT_SYMBOL(__module_address);

// is this address inside a module?
bool is_module_address(unsigned long addr)
{
	bool ret;
	ret = __module_address(addr) != NULL;

	return ret;
}

// is this address inside module code?
bool is_module_text_address(unsigned long addr)
{
	bool ret;
	ret = __module_text_address(addr) != NULL;
	return ret;
}*/

struct module *find_module(const char *name)
{
	struct module *mod;
	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		if (strcmp(mod->name, name) == 0)
			return mod;
	}
	return NULL;
}

EXPORT_SYMBOL(find_module);

static bool each_symbol_in_section(const struct symsearch *arr,
				   unsigned int arrsize,
				   struct module *owner,
				   bool(*fn) (const struct symsearch * syms,
					      struct module * owner,
					      unsigned int symnum,
					      void *data), void *data)
{
	unsigned int i, j;
	for (j = 0; j < arrsize; ++j) {
		for (i = 0; i < arr[j].stop - arr[j].start; i++)
			if (fn(&arr[j], owner, i, data))
				return 1;
	}
	return 0;
}

bool each_symbol(bool(*fn) (const struct symsearch * arr, struct module * owner,
			    unsigned int symnum, void *data), void *data)
{
	struct module *mod;
	const struct symsearch arr[] = {
		{__start___ksymtab, __stop___ksymtab, /*__start___kcrctab*/
		 NULL, 0},
	};

	if (each_symbol_in_section(arr, ARRAY_SIZE(arr), NULL, fn, data))
		return 1;

	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		struct symsearch arr[] = {
			{mod->syms, mod->syms + mod->num_syms, mod->crcs, 0},
		};
		if (each_symbol_in_section(arr, ARRAY_SIZE(arr), mod, fn, data))
			return 1;
	}
	return 0;
}

EXPORT_SYMBOL(each_symbol);

struct find_symbol_arg {
	// input
	const char *name;
	bool warn;

	// output
	struct module *owner;
	const unsigned long *crc;
	const struct kernel_symbol *sym;
};

static bool find_symbol_in_section(const struct symsearch *syms,
				   struct module *owner, unsigned int symnum,
				   void *data)
{
	struct find_symbol_arg *fsa = data;
	/*kprintf
	    ("\tfind_symbol_in_section: kernel symbol %s, searching for %s\n",
	     syms->start[symnum].name, fsa->name);*/
	if (strcmp(syms->start[symnum].name, fsa->name) != 0)
		return 0;
	fsa->owner = owner;
	fsa->crc = symversion(syms->crcs, symnum);
	fsa->sym = &syms->start[symnum];
	kprintf("\tfind_symbol_in_section: symbol %s matched\n",
		syms->start[symnum].name);
	return 1;
}

const struct kernel_symbol *find_symbol(const char *name,
					struct module **owner,
					const unsigned long **crc, bool warn)
{
	struct find_symbol_arg fsa;
	fsa.name = name;
	fsa.warn = warn;

	if (each_symbol(find_symbol_in_section, &fsa)) {
		if (owner)
			*owner = fsa.owner;
		if (crc)
			*crc = fsa.crc;
		return fsa.sym;
	}
	return NULL;
}

EXPORT_SYMBOL(find_symbol);

/*int module_get_kallsym(unsigned int symnum, unsigned long *value, char *type,
		       char *name, char *module_name, int *exported)
{
	struct module *mod;
	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		if (symnum < mod->num_symtab) {
			*value = mod->symtab[symnum].st_value;
			*type = mod->symtab[symnum].st_info;
			strncpy(name, mod->strtab + mod->symtab[symnum].st_name,
				128);
			strncpy(module_name, mod->name, MODULE_NAME_LEN);
			*exported = is_exported(name, *value, mod);
			return 0;
		}
		symnum -= mod->num_symtab;
	}
	return -1;
}*/

/*static unsigned long mod_find_symname(struct module *mod, const char *name)
{
	unsigned int i;
	for (i = 0; i < mod->num_symtab; i++)
		if (strcmp(name, mod->strtab + mod->symtab[i].st_name) == 0 &&
		    mod->symtab[i].st_info != 'U')
			return mod->symtab[i].st_value;
	return 0;
}

unsigned long module_kallsyms_lookup_name(const char *name)
{
	struct module *mod;
	char *colon;
	unsigned long ret = 0;

	if ((colon = strchr(name, ':')) != NULL) {
		*colon = '\0';
		if ((mod = find_module(name)) != NULL)
			ret = mod_find_symname(mod, colon + 1);
		*colon = ':';
	} else {
		list_entry_t *list = &(modules), *le = list;
		while ((le = list_next(le)) != list) {
			mod = le2mod(le, list);
			if ((ret = mod_find_symname(mod, name)) != 0)
				break;
		}
	}
	return ret;
}
*/
/*int module_kallsyms_on_each_symbol(int (*fn)
				    (void *, const char *, struct module *,
				     unsigned long), void *data)
{
	struct module *mod;
	unsigned int i;
	int ret;
	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		for (i = 0; i < mod->num_symtab; ++i) {
			ret =
			    fn(data, mod->strtab + mod->symtab[i].st_name, mod,
			       mod->symtab[i].st_value);
			if (ret != 0)
				return ret;
		}
	}
	return 0;

}
*/
/*unsigned int module_refcount(struct module *mod)
{
	return atomic_read(__module_ref_addr(mod, 0));
}

EXPORT_SYMBOL(module_refcount);

void __module_put_and_exit(struct module *mod, long code)
{
	module_put(mod);
	do_exit(code);
}*/

struct module_use {
	list_entry_t list;
	struct module *module_which_uses;
};

#define le2moduse(le, member)                          \
    to_struct((le), struct module_use, member)

static void module_unload_init(struct module *mod)
{
	list_init(&mod->modules_which_use_me);
	atomic_set(__module_ref_addr(mod, 0), 1);
}

static int already_uses(struct module *a, struct module *b)
{
	struct module_use *use;
	list_entry_t *list = &(b->modules_which_use_me), *le = list;
	while ((le = list_next(le)) != list) {
		use = le2moduse(le, list);
		if (use->module_which_uses == a) {
			return 1;
		}
	}
	return 0;
}

int use_module(struct module *a, struct module *b)
{
	struct module_use *use;
	int no_warn, err;
	if (b == NULL || already_uses(a, b))
		return 1;

	// TODO: interrupt or time out

	use = kmalloc(sizeof(*use));
	if (!use) {
		kprintf("%s: out of memory loading.\n", a->name);
		// TODO: module put
		return 0;
	}

	use->module_which_uses = a;
	list_add(&use->list, &b->modules_which_use_me);
	return 1;
}

EXPORT_SYMBOL(use_module);

static void module_unload_free(struct module *mod)
{
	struct module *i;
	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		i = le2mod(le, list);
		list_entry_t *list_use = &(i->modules_which_use_me), *le_use =
		    list_use;
		struct module_use *use;
		while ((le_use = list_next(le_use)) != list_use) {
			use = le2moduse(le_use, list);
			if (use->module_which_uses == mod) {
				kprintf("%s unusing %s\n", mod->name, i->name);
				list_del(&use->list);
				kfree(use);
				break;
			}
		}
	}
}

void print_modules(void)
{
	struct module *mod;
	char buf[8];

	kprintf("Modules linked in:");
	list_entry_t *list = &(modules), *le = list;
	while ((le = list_next(le)) != list) {
		mod = le2mod(le, list);
		kprintf(" %s", mod->name);
	}
	kprintf("\n");
}

static int __unlink_module(void *_mod)
{
	struct module *mod = _mod;
	list_del(&mod->list);
	return 0;
}

static void free_module(struct module *mod)
{
	__unlink_module(mod);
	module_unload_free(mod);
	module_free(mod, mod->module_init);
	module_free(mod, mod->module_core);
}

/* Additional bytes needed by arch in front of individual sections */
unsigned int __weak arch_mod_section_prepend(struct module *mod,
					     unsigned int section)
{
	/* default implementation just returns zero */
	return 0;
}

/* Update size with this section: return offset. */
static long get_offset(struct module *mod, unsigned int *size,
		       struct secthdr *sechdr, unsigned int section)
{
	long ret;
	*size += arch_mod_section_prepend(mod, section);
	ret = ALIGN(*size, sechdr->sh_addralign ? : 1);
	*size = ret + sechdr->sh_size;
	return ret;
}

static void layout_sections(struct module *mod,
			    const struct elfhdr *hdr,
			    struct secthdr *sechdrs, const char *secstrings)
{
	static unsigned long const masks[][2] = {
		{SHF_EXECINSTR | SHF_ALLOC, ARCH_SHF_SMALL},
		{SHF_ALLOC, SHF_WRITE | ARCH_SHF_SMALL},
		{SHF_WRITE | SHF_ALLOC, ARCH_SHF_SMALL},
		{ARCH_SHF_SMALL | SHF_ALLOC, 0}
	};

	unsigned int m, i;

	for (i = 0; i < hdr->e_shnum; i++) {
		sechdrs[i].sh_entsize = ~0UL;
	}

	kprintf("layout_sections: Core section allocation order:\n");
	for (m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (i = 0; i < hdr->e_shnum; ++i) {
			struct secthdr *s = &sechdrs[i];

			if ((s->sh_flags & masks[m][0]) != masks[m][0]
			    || (s->sh_flags & masks[m][1])
			    || s->sh_entsize != ~0UL
			    || strncmp(secstrings + s->sh_name, ".init",
				       strlen(".init")) == 0)
				continue;
			s->sh_entsize = get_offset(mod, &mod->core_size, s, i);
			kprintf("\t%s\n", secstrings + s->sh_name);
		}
		if (m == 0)
			mod->core_text_size = mod->core_size;
	}

	kprintf("layout_sections: Init section allocation order:\n");
	for (m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (i = 0; i < hdr->e_shnum; ++i) {
			struct secthdr *s = &sechdrs[i];

			if ((s->sh_flags & masks[m][0]) != masks[m][0]
			    || (s->sh_flags & masks[m][1])
			    || s->sh_entsize != ~0UL
			    || strncmp(secstrings + s->sh_name, ".init",
				       strlen(".init")) != 0)
				continue;
			s->sh_entsize = get_offset(mod, &mod->init_size, s, i)
			    | INIT_OFFSET_MASK;
			kprintf("\t%s\n", secstrings + s->sh_name);
		}
		if (m == 0)
			mod->init_text_size = mod->init_size;
	}
}

static void *module_alloc_update_bounds(unsigned long size)
{
	void *ret = module_alloc(size);;
	if (ret) {
		if ((unsigned long)ret < module_addr_min)
			module_addr_min = (unsigned long)ret;
		if ((unsigned long)ret + size > module_addr_max)
			module_addr_max = (unsigned long)ret + size;
	}
	return ret;
}

static int verify_export_symbols(struct module *mod)
{
	unsigned int i;
	struct module *owner;
	const struct kernel_symbol *s;
	struct {
		const struct kernel_symbol *sym;
		unsigned int num;
	} arr[] = {
		{
	mod->syms, mod->num_syms},};

	for (i = 0; i < ARRAY_SIZE(arr); i++) {
		for (s = arr[i].sym; s < arr[i].sym + arr[i].num; s++) {
			if (find_symbol(s->name, &owner, NULL, 0)) {
				kprintf("verify_export_symbols: "
					"%s: exports duplicate symbol %s"
					" (owned by %s)\n",
					mod->name, s->name, module_name(owner));
				return -1;
			}
		}
	}
	return 0;
}

static int simplify_symbols(struct secthdr *sechdrs,
			    unsigned int symindex,
			    const char *strtab,
			    unsigned int versindex,
			    unsigned int pcpuindex, struct module *mod)
{
	struct symtab_s *sym = (void *)sechdrs[symindex].sh_addr;
	unsigned long secbase;
	unsigned int i, n = sechdrs[symindex].sh_size / sizeof(struct symtab_s);
	int ret = 0;
	const struct kernel_symbol *ksym;
	for (i = 1; i < n; i++) {
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			/* We compiled with -fno-common.  These are not supposed to happen.  */
			kprintf("simplify_symbols: Common symbol: %s\n",
				strtab + sym[i].st_name);
			kprintf("%s: please compile with -fno-common\n",
				mod->name);
			ret = -1;
			break;
		case SHN_ABS:
			/* Don't need to do anything */
			kprintf("simplify_symbols: Absolute symbol: 0x%08lx\n",
				(long)sym[i].st_value);
			break;
		case SHN_UNDEF:
			ksym =
			    resolve_symbol(sechdrs, versindex,
					   strtab + sym[i].st_name, mod);
			/* Ok if resolved.  */
			if (ksym) {
				sym[i].st_value = ksym->value;
				break;
			}
			/* Ok if weak. */
			if (ELF_ST_BIND(sym[i].st_info) == STB_WEAK)
				break;
			kprintf("simplify_symbols: Unknown symbol %s\n",
				strtab + sym[i].st_name);
			ret = -1;
			break;
		default:
			if (sym[i].st_shndx == pcpuindex)
				secbase = (unsigned long)mod->percpu;
			else
				secbase = sechdrs[sym[i].st_shndx].sh_addr;
			sym[i].st_value += secbase;
			break;
		}
	}
	return ret;
}

//static const char vermagic[] = "";

static noinline struct module *load_module(void __user * umod,
					   unsigned long len,
					   const char __user * uargs)
{
	struct elfhdr *hdr;
	struct secthdr *sechdrs;
	char *secstrings, *args, *modmagic, *strtab = NULL;
	//char *staging;

	unsigned int i;
	unsigned int symindex = 0;
	unsigned int strindex = 0;
	unsigned int modindex, versindex, infoindex, pcpuindex;
	struct module *mod;
	long err = 0;
	void *ptr = NULL;

	kprintf("load_module: umod=%p, len=%lu, uargs=%p\n", umod, len, uargs);

	if (len < sizeof(*hdr))
		return NULL;
	if (len > 64 * 1024 * 1024 || (hdr = kmalloc(len)) == NULL)
		return NULL;

	kprintf("load_module: copy_from_user\n");

	struct mm_struct *mm = current->mm;
	lock_mm(mm);
	if (!copy_from_user(mm, hdr, umod, len, 1)) {
		unlock_mm(mm);
		goto free_hdr;
	}
	unlock_mm(mm);

	kprintf("load_module: hdr:%p\n", hdr);
	// sanity check
	if (memcmp(&(hdr->e_magic), ELFMAG, SELFMAG) != 0
	    || hdr->e_type != ET_REL || !elf_check_arch(hdr)
	    || hdr->e_shentsize != sizeof(*sechdrs)) {
		kprintf("load_module: sanity check failed.\n");
		goto free_hdr;
	}

	if (len < hdr->e_shoff + hdr->e_shnum * sizeof(*sechdrs))
		goto truncated;

	sechdrs = (void *)hdr + hdr->e_shoff;
	secstrings = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
	sechdrs[0].sh_addr = 0;

	for (i = 1; i < hdr->e_shnum; i++) {
		if (sechdrs[i].sh_type != SHT_NOBITS
		    && len < sechdrs[i].sh_offset + sechdrs[i].sh_size)
			goto truncated;

		// mark sh_addr
		sechdrs[i].sh_addr = (size_t) hdr + sechdrs[i].sh_offset;

		if (sechdrs[i].sh_type == SHT_SYMTAB) {
			symindex = i;
			strindex = sechdrs[i].sh_link;
			strtab = (char *)hdr + sechdrs[strindex].sh_offset;
		}

	}

	modindex =
	    find_sec(hdr, sechdrs, secstrings, ".gnu.linkonce.this_module");

	if (!modindex) {
		kprintf("load_module: No module found in object.\n");
		goto free_hdr;
	}
	// temp: point mod into copy of data
	mod = (void *)sechdrs[modindex].sh_addr;

	if (symindex == 0) {
		kprintf("load_module: %s module has no symbols (stripped?).\n",
			mod->name);
		goto free_hdr;
	}
	versindex = find_sec(hdr, sechdrs, secstrings, "__versions");
	infoindex = find_sec(hdr, sechdrs, secstrings, ".modinfo");
	pcpuindex = 0;//find_pcpusec(hdr, sechdrs, secstrings);

	// don't keep modinfo and version
	sechdrs[infoindex].sh_flags &= ~(unsigned long)SHF_ALLOC;
	sechdrs[versindex].sh_flags &= ~(unsigned long)SHF_ALLOC;

	// keep symbol and string tables
	sechdrs[symindex].sh_flags |= SHF_ALLOC;
	sechdrs[strindex].sh_flags |= SHF_ALLOC;

	/*if (!check_modstruct_version(sechdrs, versindex, mod)) {
		goto free_hdr;
	}*/

	/*
	   modmagic = get_modinfo(sechdrs, infoindex, "vermagic");

	   if (!modmagic) {
	   kprintf("load_module: bad vermagic\n");
	   goto free_hdr;
	   } else if (!same_magic(modmagic, vermagic, versindex)) {
	   ; 
	   // TODO: module magic is left for future use.
	   }
	 */

	//staging = get_modinfo(sechdrs, infoindex, "staging");
	// TODO: staging is left for future use.

	if (find_module(mod->name)) {
		kprintf("load_module: module %s exists\n", mod->name);
		goto free_mod;
	}

	mod->state = MODULE_STATE_COMING;

	// err = module_frob_arch_sections(hdr, sechdrs, secstrings, mod);
	// TODO: we do not need it for x86 or arm

	// TODO: percpu is no longer needed.

	layout_sections(mod, hdr, sechdrs, secstrings);

	ptr = module_alloc_update_bounds(mod->core_size);

	if (!ptr) {
		goto free_percpu;
	}
	memset(ptr, 0, mod->core_size);
	mod->module_core = ptr;

	ptr = module_alloc_update_bounds(mod->init_size);

	if (!ptr && mod->init_size) {
		goto free_core;
	}
	memset(ptr, 0, mod->init_size);
	mod->module_init = ptr;

	kprintf("load_module: final section addresses:\n");
	for (i = 0; i < hdr->e_shnum; i++) {
		void *dest;
		if (!(sechdrs[i].sh_flags & SHF_ALLOC)) {
			kprintf("\tSkipped %s\n",
				secstrings + sechdrs[i].sh_name);
			continue;
		}
		if (sechdrs[i].sh_entsize & INIT_OFFSET_MASK)
			dest =
			    mod->module_init +
			    (sechdrs[i].sh_entsize & ~INIT_OFFSET_MASK);
		else
			dest = mod->module_core + sechdrs[i].sh_entsize;
		if (sechdrs[i].sh_type != SHT_NOBITS)
			memcpy(dest, (void *)sechdrs[i].sh_addr,
			       sechdrs[i].sh_size);
		sechdrs[i].sh_addr = (unsigned long)dest;
		kprintf("\t0x%lx %s\n", sechdrs[i].sh_addr,
			secstrings + sechdrs[i].sh_name);
	}
	/* Module has been moved. */
	mod = (void *)sechdrs[modindex].sh_addr;

	/* Now we've moved module, initialize linked lists, etc. */
	module_unload_init(mod);

	/* Set up license info based on the info section */
	set_license(mod, get_modinfo(sechdrs, infoindex, "license"));

	err = simplify_symbols(sechdrs, symindex, strtab, versindex, pcpuindex,
			       mod);

	if (err < 0)
		goto cleanup;

	mod->syms = section_objs(hdr, sechdrs, secstrings, "__ksymtab",
				 sizeof(*mod->syms), &mod->num_syms);
	mod->crcs = section_addr(hdr, sechdrs, secstrings, "__kcrctab");

	// relocations
	for (i = 1; i < hdr->e_shnum; i++) {
		const char *strtab = (char *)sechdrs[strindex].sh_addr;
		unsigned int info = sechdrs[i].sh_info;

		/* Not a valid relocation section */
		if (info >= hdr->e_shnum)
			continue;

		/* Don't bother with non-allocated sections */
		if (!(sechdrs[info].sh_flags & SHF_ALLOC))
			continue;

		if (sechdrs[i].sh_type == SHT_REL)
			err = apply_relocate(sechdrs, strtab, symindex, i, mod);
		else if (sechdrs[i].sh_type == SHT_RELA)
			err =
			    apply_relocate_add(sechdrs, strtab, symindex, i,
					       mod);

		if (err < 0)
			goto cleanup;
	}

	err = verify_export_symbols(mod);
	if (err < 0)
		goto cleanup;

	// TODO: kallsyms is left for future use.
	//add_kallsyms(mod, sechdrs, symindex, strindex, secstrings);

	err = module_finalize(hdr, sechdrs, mod);
	if (err < 0)
		goto cleanup;

	list_add(&modules, &mod->list);

	kfree(hdr);
	return mod;

cleanup:
	module_unload_free(mod);

free_init:
	module_free(mod, mod->module_init);

free_core:
	module_free(mod, mod->module_core);

free_percpu:

free_mod:

free_hdr:
	kfree(hdr);
	return NULL;

truncated:
	kprintf("load_module: module len %lu truncated.\n");
	goto free_hdr;
}

int do_init_module(void __user * umod, unsigned long len,
		   const char __user * uargs)
{
	struct module *mod;
	int ret = 0;

	// TODO: non-preemptive kernel does not need to lock module mutex

	mod = load_module(umod, len, uargs);
	if (mod == NULL) {
		// TODO: non-preemptive kernel does not need to unlock module mutex
		return -1;
	}
	// TODO: non-preemptive kernel does not need to unlock module mutex

	if (mod->init != NULL)
		ret = (*mod->init) ();
	if (ret < 0) {
		mod->state = MODULE_STATE_GOING;
		// TODO: non-preemptive kernel does not need to lock module mutex
		free_module(mod);
		// TODO: non-preemptive kernel does not need to unlock
		return ret;
	}
	if (ret > 0) {
		kprintf("%s: %s->init suspiciously returned %d\n"
			"%s: loading anyway...\n", __func__, mod->name, ret,
			__func__);
	}
	mod->state = MODULE_STATE_LIVE;

	// TODO: lock?
	module_free(mod, mod->module_init);
	mod->module_init = NULL;
	mod->init_size = 0;
	mod->init_text_size = 0;
	// TODO: unlock

	return 0;
}

int do_cleanup_module(const char __user * name_user)
{
	struct module *mod;
	char name[MODULE_NAME_LEN];
	int ret = 0, forced = 0;

	struct mm_struct *mm = current->mm;
	lock_mm(mm);

	int length = strlen(name_user);
	if (!copy_from_user(mm, name, name_user, length, 1)) {
		unlock_mm(mm);
		return -E_INVAL;
	}
	unlock_mm(mm);
	name[length] = '\0';

	mod = find_module(name);

	if (!mod) {
		ret = -E_NOENT;
		goto out;
		//return -E_NOENT;
	}
	if (!list_empty(&mod->modules_which_use_me)) {
		ret = -E_INVAL;
		goto out;
	}

	if (mod->state != MODULE_STATE_LIVE) {
		kprintf("do_cleanup_module: %s already dying\n", mod->name);
		ret = -E_BUSY;
	}

	if (mod->init && !mod->exit) {
		kprintf("do_cleanup_module: %s can't be removed\n", mod->name);
		ret = -E_BUSY;
	}

	mod->waiter = current;
	mod->state = MODULE_STATE_GOING;

	if (mod->exit != NULL)
		mod->exit();

	strncpy(last_unloaded_module, mod->name, strlen(mod->name));
	free_module(mod);
	return ret;

out:
	return ret;

}

int module_finalize(const struct elfhdr *hdr,
		    const struct secthdr *sechdrs, struct module *mod)
{
	const struct secthdr *s, *text = NULL, *alt = NULL, *locks =
	    NULL, *para = NULL;
	char *secstrings = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
	for (s = sechdrs; s < sechdrs + hdr->e_shnum; s++) {
		if (!strcmp(".text", secstrings + s->sh_name))
			text = s;
		if (!strcmp(".altinstructions", secstrings + s->sh_name))
			alt = s;
		if (!strcmp(".smp_locks", secstrings + s->sh_name))
			locks = s;
		if (!strcmp(".parainstructions", secstrings + s->sh_name))
			para = s;
	}
	if (alt) {
		kprintf("apply_relocate_add: patch .altinstructions");
	}
	if (locks && text) {
		kprintf("apply_relocate_add: alternatives_smp_module_add");
	}
	if (para) {
		kprintf("apply_relocate_add: patch .parainstructions");
	}
	return 0;
}

void mod_init()
{
}


EXPORT_SYMBOL(hash32);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(inode_init);
EXPORT_SYMBOL(vfs_mount);
EXPORT_SYMBOL(__alloc_fs);
EXPORT_SYMBOL(__alloc_inode);
EXPORT_SYMBOL(__warn);
EXPORT_SYMBOL(iobuf_skip);
EXPORT_SYMBOL(ide_read_secs);
EXPORT_SYMBOL(null_vop_unimp);
EXPORT_SYMBOL(kmalloc);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(unregister_filesystem);
EXPORT_SYMBOL(inode_check);
EXPORT_SYMBOL(kfree);
EXPORT_SYMBOL(register_filesystem);
EXPORT_SYMBOL(ide_write_secs);
EXPORT_SYMBOL(null_vop_notdir);
EXPORT_SYMBOL(inode_ref_inc);
EXPORT_SYMBOL(__panic);
EXPORT_SYMBOL(inode_ref_dec);
EXPORT_SYMBOL(null_vop_inval);
EXPORT_SYMBOL(stricmp);
EXPORT_SYMBOL(iobuf_move);
EXPORT_SYMBOL(null_vop_isdir);
