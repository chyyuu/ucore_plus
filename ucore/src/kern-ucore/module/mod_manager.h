#ifndef __KERN_MODULE_MOD_MANAGER_H__
#define  __KERN_MODULE_MOD_MANAGER_H__

#include <mod.h>

int add_module(const char *name, struct elf_mod_info_s * info);
int del_module(const char *name);
int module_loaded(const char *name);
void print_loaded_module();
struct elf_mod_info_s * get_module(const char *name);

#endif
