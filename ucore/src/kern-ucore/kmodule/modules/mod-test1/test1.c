#include <kio.h>
#include <mod.h>
#include <string.h>
int test1(void)
{
    return 10;
}
EXPORT_SYMBOL(test1);

static __init int test1_init() {
    return 0;
}

static __exit void test1_exit() {
}

module_init(test1_init);
module_exit(test1_exit);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "mod-test1",
    .init = test1_init,
    .exit = test1_exit,
};
