#include <ulib.h>
#include <stdio.h>
#include <mount.h>
#include <string.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)

void
usage(void) {
    printf("usage: mount -t filesystem device\n");
}

int
check(int argc, char **argv)
{
	if (argc != 4) {
        return -1;
    }
    if (strcmp(argv[1], "-t") != 0) {
    	return -1;
    }
    return 0;
}

int
main(int argc, char **argv) {
	int ret;
    if (check(argc, argv) != 0) {
        usage();
        return -1;
    }
    else {
        ret = mount(argv[3], NULL, argv[2], NULL);
    }

    if (!ret) {
    	printf("mounted file system %s on %s.\n", argv[2], argv[3]);
    }
    else {
    	printf("mount failed.\n");
    }

    return ret;
}

