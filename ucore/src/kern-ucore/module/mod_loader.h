#ifndef __KERN_MODULE_MODLOADER_H__
#define  __KERN_MODULE_MODLOADER_H__

void mod_loader_init();
int find_export_sym(const char *name, int touch);
void mod_touch_symbol(const char *name, void * ptr, uint32_t flags);
void mod_disable_symbol(const char *name);
void touch_export_sym(const char *name, uintptr_t ptr, uint32_t flags);
uintptr_t get_sym_ptr(int idx);

#define error(x ...) kprintf("[ EE ] %s:%d, ", __FILE__, __LINE__);kprintf(x)

//#define DEBUG_KERN_MODULE
#ifdef DEBUG_KERN_MODULE
    #define info(x ...) kprintf("[ II ] ");kprintf(x)
#else
    #define info(x ...)
#endif

#endif
