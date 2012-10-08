#ifndef __KERN_MODULE_MOD_H__
#define  __KERN_MODULE_MOD_H__

#include <stdio.h>

#define MOD_INIT_MODULE "init_module"
#define MOD_CLEANUP_MODULE "cleanup_module"

#define MOD_ADD "module_func_add"
#define MOD_MUL "module_func_mul"

typedef int (*func_add_t)(int a, int b, int *c);
typedef int (*func_mul_t)(int a, int b, int *c);

struct elf_mod_info_s {
     uintptr_t image;
     uint32_t  image_size;
     
     uintptr_t ptr;
     uintptr_t common_ptr;
     uint32_t  common_size;
     uintptr_t load_ptr;
     uintptr_t unload_ptr;
};

void mod_init();
int elf_mod_load(uintptr_t image, uint32_t image_size, struct elf_mod_info_s * info);

int module_func_add(int a, int b, int *c);
int module_func_mul(int a, int b, int *c);

void register_mod_add(func_add_t f);
void unregister_mod_add();

void register_mod_mul(func_mul_t f);
void unregister_mod_mul();

int do_init_module(const char *name);
int do_cleanup_module(const char *name);

int do_mod_add(int a, int b);
int do_mod_mul(int a, int b);

#endif

