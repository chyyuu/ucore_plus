#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <mod.h>
#include <file.h>
#include <stat.h>
#include <malloc.h>
#include <unistd.h>

#define MX_MOD_PATH_LEN 1024
static char path[MX_MOD_PATH_LEN];

#define KERN_MODULE_PREFIX "/lib/modules/"
#define KERN_MODULE_SUFFIX ".ko"
// must be the char count sum of the 2 strings above
#define KERN_MODULE_PREFIX_LEN 13
#define KERN_MODULE_ADDITIONAL_LEN 16

#define KERN_MODULE_DEP KERN_MODULE_PREFIX"mod.dep"

#define USAGE "insmod <mod-name>\n"
#define LOADING "loading module "

int in_short(const char *name, int size) {
    return size < 3 || strcmp(KERN_MODULE_SUFFIX, &name[size - 3]) != 0;
}

char * get_short_name(char *name) {
    int namelen = strlen(name);
    if (in_short(name, namelen)) {
        return name;
    }
    int size = namelen - KERN_MODULE_ADDITIONAL_LEN;
    char * ret = (char *)malloc(size + 1);
    memcpy(ret, &name[KERN_MODULE_PREFIX_LEN], size);
    ret[size] = '\0';
    return ret;
}

void load_mod(const char *name)
{
	int fd;
	int ret;
	fd = open(name, O_RDONLY);
	if (fd < 0) {
		cprintf("cannot find kobject file. %s\n");
		return;
	}
	struct stat mod_stat;
	memset(&mod_stat, 0, sizeof(mod_stat));
	fstat(fd, &mod_stat);
	if (mod_stat.st_size <= 0 || mod_stat.st_size > (1<<20)) {
        cprintf("wrong obj file size: %d\n", mod_stat.st_size);
        return;
    }
	cprintf("loading kern module: %s, size = %d\n", name, mod_stat.st_size);
	void *buffer = malloc(mod_stat.st_size);
	if (buffer == NULL) {
		cprintf("not enough memory to load the module\n");
        return;
	}
	int copied = read(fd, buffer, mod_stat.st_size);
    cprintf("module size: %d, mem addr: %x\n", copied, buffer);
	ret = init_module(buffer, copied, NULL);
	free(buffer);
}
	

void load(const char *name, int size) {
    if (in_short(name, size)) {
        snprintf(path, size + KERN_MODULE_ADDITIONAL_LEN + 1, KERN_MODULE_PREFIX"%s"KERN_MODULE_SUFFIX, name);
        write(1, LOADING, strlen(LOADING));
        write(1, path, strlen(path));
        write(1, "\n", 1);
        load_mod(name);
    } else {
        load_mod(name);
    }
}

/**
 * return handled or not
 * 
 */
int handle_dep(const char *line, const char *name) {
    int name_len = strlen(name);
    int line_len = strlen(line);
    if (line_len < name_len + 1) {
        return 0;
    }
    int i;
    for (i = 0; i < name_len; i++) {
        if (name[i] != line[i]) {
            return 0;
        }
    }
    if (line[name_len] != ':') {
        return 0;
    }
    cprintf("[ II ] module dependency rule match: %s\n", line);
    int j = 0;
    for (i = name_len + 1; i < line_len; ) {
        if (line[i] == ' ') {
            i++;
            continue;
        }
        for (j = i + 1; j < line_len; j++) {
            if (line[j] == ' ') {
                break;
            }
        }
        load(&line[i], j - i);
        i = j + 1;
    }
    return 1;
}

void load_dep(const char *name) {
    char buffer[1024];
    int fd = open(KERN_MODULE_DEP, O_RDONLY);
    int s = 0;
    int p = 0;
    char c;
    while ((s = read(fd, &c, sizeof(char))) >= 0) {
        if (s == 0 || c == '\n' || c == '\r') {
            buffer[p] = '\0';
            if (p > 0) {
                int handled = handle_dep(buffer, name);
                if (handled) {
                    return;
                }
            }
            p = 0;
        } else {
            buffer[p++] = c;
        }
        if (s == 0) {
            break;
        }
    }
}

int
main(int argc, char **argv) {
    if (argc <= 1) {
        write(1, USAGE, strlen(USAGE));
        return 0;
    }
    char * short_name = get_short_name(argv[1]);
    load_dep(short_name);
    load(argv[1], strlen(argv[1]));
    return 0;
}

