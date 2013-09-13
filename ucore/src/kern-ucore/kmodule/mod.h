#ifndef __KERN_MODULE_MOD_H__
#define  __KERN_MODULE_MOD_H__

#include <stdio.h>
#include <list.h>
#include <elf.h>
#include <atomic.h>

#define __used __attribute__((__used__))
#define __weak __attribute__((weak))
#define noinline __attribute__((noinline))
//#define __user __attribute__((noderef, address_space(1)))
#define __user

#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)
#define __must_be_array(a) \
	BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))

#define __init          __section(.init.text)
#define __initdata      __section(.init.data)
#define __exitdata      __section(.exit.data)
#define __exit_call     __used __section(.exitcall.exit)
#define __exit          __section(.exit.text)

typedef int (*initcall_t) (void);
typedef void (*exitcall_t) (void);

#define __section(S) __attribute__((__section__(#S)))

#define __define_initcall(level,fn) \
	static initcall_t __initcall_##fn __used \
	__attribute__((__section__(".initcall" level ".init"))) = fn

#define __initcall(fn) __define_initcall("1", fn)
#define __exitcall(fn) static exitcall_t __exitcall_##fn __exit_call = fn

#define module_init(x) __initcall(x)
#define module_exit(x) __exitcall(x)

#define MOD_ADD "module_func_add"
#define MOD_MUL "module_func_mul"

#ifndef MODULE_SYMBOL_PREFIX
#define MODULE_SYMBOL_PREFIX ""
#endif

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define MAX_PARAM_PREFIX_LEN (64 - sizeof(unsigned long))
#define MODULE_NAME_LEN MAX_PARAM_PREFIX_LEN

typedef int (*func_add_t) (int a, int b, int *c);
typedef int (*func_mul_t) (int a, int b, int *c);

struct kernel_symbol {
	unsigned long value;
	const char *name;
};

struct modversion_info {
	unsigned long crc;
	char name[MODULE_NAME_LEN];
};

struct mod_arch_specific {
};

/*
 * We don't need these. They are obsolete.

extern int init_module(void);
extern void cleanup_module(void);
 */

#define __EXPORT_SYMBOL(sym, sec) 				\
	extern typeof(sym) sym; 			\
	static const char __kstrtab_##sym[] \
	__attribute__((section("__ksymtab_strings"), aligned(1))) \
	= MODULE_SYMBOL_PREFIX #sym; 		\
	static const struct kernel_symbol __ksymtab_##sym \
	__used								\
	__attribute__((section("__ksymtab" sec), unused)) \
	= { (unsigned long)&sym, __kstrtab_##sym }

#define EXPORT_SYMBOL(sym) __EXPORT_SYMBOL(sym, "")

enum module_state {
	MODULE_STATE_LIVE,
	MODULE_STATE_COMING,
	MODULE_STATE_GOING,
};

struct module {
	enum module_state state;

	// members of list of modules
	list_entry_t list;

	// unique handle for this module
	char name[MODULE_NAME_LEN];

	// exported symbols
	const struct kernel_symbol *syms;
	const unsigned long *crcs;
	unsigned int num_syms;

	// startup function
	int (*init) (void);

	// if non-NULL, free after init() returns
	void *module_init;

	// actual code + date, free on unload
	void *module_core;

	unsigned int init_size, core_size;

	unsigned int init_text_size, core_text_size;

	struct mod_arch_specific arch;

	unsigned int taints;

	// for kallsyms
	struct symtab_s *symtab;
	unsigned int num_symtab;
	char *strtab;

	// omitted section and notes attributes
	void *percpu;

	// omitted markers and tracing

	list_entry_t modules_which_use_me;

	// TODO: task waiter
	struct proc_struct *waiter;

	// destruction function
	void (*exit) (void);

	atomic_t ref;
};

#define le2mod(le, member)                          \
    to_struct((le), struct module, member)

#define MODULE_GENERIC_TABLE(gtype, name)		\
extern const struct gtype##_id __mod_##gtype##_table	\
	__attribute__ ((unused, alias(__stringify(name))))

extern struct module __this_module;
#define THIS_MODULE (&__this_module)

// TODO:
// extern semaphore_t module_mutex_sem;

static inline int module_is_live(struct module *mod)
{
	return mod->state != MODULE_STATE_GOING;
}

//struct module *__module_text_address(unsigned long addr);
//struct module *__module_address(unsigned long addr);
//bool is_module_address(unsigned long addr);
//bool is_module_text_address(unsigned long addr);

static inline int within_module_core(unsigned long addr, struct module *mod)
{
	return (unsigned long)mod->module_core <= addr &&
	    addr < (unsigned long)mod->module_core + mod->core_size;
}

static inline int within_module_init(unsigned long addr, struct module *mod)
{
	return (unsigned long)mod->module_init <= addr &&
	    addr < (unsigned long)mod->module_init + mod->core_size;
}

// TODO: hold module_mutex_sem before search
struct module *find_module(const char *name);

struct symsearch {
	const struct kernel_symbol *start, *stop;
	const unsigned long *crcs;
	bool unused;
};

// search for an exported symbol by name
const struct kernel_symbol *find_symbol(const char *name, struct module **owner,
					const unsigned long **crc, bool warn);

// walk the exported symbol table
bool each_symbol(bool(*fn)
		  (const struct symsearch * arr, struct module * owner,
		   unsigned int symnum, void *data), void *data);

//int module_get_kallsym(unsigned int symnum, unsigned long *value, char *type,
//		       char *name, char *module_name, int *exported);

//unsigned long module_kallsyms_lookup_name(const char *name);

/*int module_kallsyms_on_each_symbol(int (*fn)
				    (void *, const char *, struct module *,
				     unsigned long), void *data);
*/
// TODO: SMP?
//extern void __module_put_and_exit(struct module *mod, long code);
//#define module_put_and_exit(code) __module_put_and_exit(THIS_MODULE, code)

// TODO: UNLOAD
// START
//unsigned int module_refcount(struct module *mod);

static inline atomic_t *__module_ref_addr(struct module *mod, int cpu)
{
	return &mod->ref;
}

static inline int try_module_get(struct module *module)
{
	return !module || module_is_live(module);
}

static inline void module_put(struct module *module)
{
}

static inline void __module_get(struct module *module)
{
	if (module) {
		atomic_inc(__module_ref_addr(module, 0));
	}
}

#define symbol_put(x) do {} while (0)
#define symbol_put_addr(p) do {} while (0)

// END

int use_module(struct module *a, struct module *b);

#define module_name(mod)	\
({							\
	struct module *__mod = (mod);	\
	__mod ? __mod->name : "kernel";	\
})

// TODO:
const char *module_address_lookup_(unsigned long addr,
				   unsigned long *symbolsize,
				   unsigned long *offset,
				   char **modname, char *namebuf);

int lookup_module_symbol_name_(unsigned long addr, char *symname);
int lookup_module_symbol_attrs_(unsigned long addr, unsigned long *size,
				unsigned long *offset, char *modname,
				char *name);

const struct exception_table_entry *search_module_extables_(unsigned long addr);

extern void print_modules(void);

void mod_init();

int do_init_module(void __user * umod, unsigned long len,
		   const char __user * uargs);
int do_cleanup_module(const char __user * name_user);

#endif
