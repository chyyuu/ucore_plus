#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <mod.h>
#include <file.h>

#define MX_MOD_PATH_LEN 1024
static char path[MX_MOD_PATH_LEN];

#define KERN_MODULE_PREFIX "/kern-module/"
#define KERN_MODULE_SUFFIX ".ko"
// must be the char count sum of the 2 strings above
#define KERN_MODULE_ADDITIONAL_LEN 16

#define USAGE "rmmod <mod-name>\n"
#define LOADING "removing module "

int
main(int argc, char **argv) {
    if (argc <= 1) {
        write(1, USAGE, strlen(USAGE));
        return 0;
    }
    int size = strlen(argv[1]);
    char * param = argv[1];
    if (size < 3 || strcmp(KERN_MODULE_SUFFIX, &param[size - 3]) != 0) {        // shortcut mode
        snprintf(path, size + KERN_MODULE_ADDITIONAL_LEN + 1, KERN_MODULE_PREFIX"%s"KERN_MODULE_SUFFIX, param);
        write(1, LOADING, strlen(LOADING));
        write(1, path, strlen(path));
        write(1, "\n", 1);
        cleanup_module(path);
    } else {
        cleanup_module(param);
    }
    return 0;
}

