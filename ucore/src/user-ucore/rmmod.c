#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <mod.h>
#include <file.h>


#define USAGE "rmmod <mod-name>\n"
#define LOADING "Removing module "

int
main(int argc, char **argv) {
    if (argc <= 1) {
        cprintf(USAGE);
        return 0;
    }
    int size = strlen(argv[1]);
    char *name = argv[1];
    cprintf(LOADING "%s\n", name);
    int ret = cleanup_module(name);
    if (ret < 0)
        cprintf("Failed to remove %s.\n", name);
    return 0;
}

