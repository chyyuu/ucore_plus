#include <syscall.h>

int
main(int argc, char **argv) {
    sys_list_module();
    return 0;
}

