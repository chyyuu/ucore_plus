#include <kio.h>
#include <mod.h>

extern int (*add_func)(int x, int y);

int add(int a, int b) {
    return a + b;
}

static __init int add_init() {
    kprintf("add_init: Hello world!\n");
    add_func = &add;
    return 0;
}

static __exit void add_exit() {
    add_func = NULL;
    kprintf("add_init: Goodbye, cruel world.\n");
}

module_init(add_init);
module_exit(add_exit);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "mod-add",
    .init = add_init,
    .exit = add_exit,
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) = 
"depends=";