#include <kio.h>
#include <mod.h>

int test;
int global;

int add(int a, int b, int *c) {
    *c = a + b;
    return 0;
}

void init_module() {
    kprintf("[ MM ] init mod-add\n");
    register_mod_add(add);
    kprintf("[ MM ] init mod-add done\n");
    test = 1;
    global = 10 + test;
    kprintf("test: %d, global: %d\n", test, global);
}

void cleanup_module() {
    kprintf("[ MM ] cleanup module mod-add\n");
    unregister_mod_add();
    kprintf("[ MM ] cleanup module mod-add done\n");
}
