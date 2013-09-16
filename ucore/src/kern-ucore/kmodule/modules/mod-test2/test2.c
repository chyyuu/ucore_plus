#include <kio.h>
#include <mod.h>
#include <string.h>
extern int test1(void);

int test2_init(void)
{
    printk("get from test1(): %d\n", test1());
    return 0;
}


static __exit void test2_exit() {
}

module_init(test2_init);
module_exit(test2_exit);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "mod-test2",
    .init = test2_init,
    .exit = test2_exit,
};
