#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <mod.h>
#include <unistd.h>
#include <syscall.h>
#include <file.h>

#define USAGE "test_mod_mul <int1> <int2>\n"

int isdigit(char c) {
    return c >= '0' && c <= '9';
}

int atoi(const char * s) {
    const char * p;
    int ret = 0;
    char sign = 1;
    for (p = s; *p; p++) {
        if (isdigit(*p)) {
            ret = ret * 10 + (*p - '0');
        } else if (*p == '-') {
            sign = 0;
        }
    }
    return sign ? ret : -ret;
}

int
main(int argc, char **argv) {
    if (argc <= 2) {
        write(1, USAGE, strlen(USAGE));
        return 0;
    }
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int c = sys_mod_mul(a, b);
    char rs[30];
    snprintf(rs, 30, "%d * %d = %d\n", a, b, c);
    write(1, rs, strlen(rs));
    return 0;
}
