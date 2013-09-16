#include <kio.h>
#include <types.h>
#include <mod.h>
extern int (*add_func)(int x, int y);
extern int (*mul_func)(int x, int y);
int mul(int a, int b) {
    if (add_func == NULL)
        return -1;
    if (a > b) {
        int tmp = b;
        b = a;
        a = tmp;
    }
	int i = 0;
	int c = 0;
    for (; i < a; i++)
        c=add_func(b, c);
    return c;
}

static __init int mul_init() {
    kprintf("mul_init: Hello world!\n");
    mul_func = &mul;
    return 0;
}
	
static __exit void mul_exit() {
    mul_func = NULL;
    kprintf("mul_exit: Goodbye, cruel world.\n");
}
	
module_init(mul_init);
module_exit(mul_exit);
	
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "mod-mul",
    .init = mul_init,
    .exit = mul_exit,
};
	
static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
	"depends=";
