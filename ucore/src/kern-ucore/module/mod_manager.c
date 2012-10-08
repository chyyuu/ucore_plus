#include <string.h>
#include <stdio.h>
#include <slab.h>
#include <mod.h>
#include <kio.h>

#define uint32_t unsigned int
#define MX 97
#define output(x...) kprintf(x)

char * table[MX];
struct elf_mod_info_s * modules[MX];

static uint32_t hash(const char * str) {
    uint32_t hash = 0;
    while(*str) {
        hash = (*str++) + (hash << 6) + (hash << 16) - hash;
    }
    return (hash & 0x7fffffff) % MX;
}

static int hash_locate(const char *str) {
    uint32_t loc = hash(str);
    uint32_t ori = loc;
    while (table[loc] && strcmp(table[loc], str)) {
        loc++;
        if (loc >= MX) {
            loc %= MX;
        }
        if (loc == ori) {
            return -1;
        }
    }
    return loc;
}

/**
 *  find the str in hash table, return -1 if not found, else return its index in table
 */
static int hash_find(const char *str) {
    uint32_t loc = hash(str);
    uint32_t ori = loc;
    while (1) {
        if (!table[loc]) {
            return -1;
        }
        if (strcmp(table[loc], str) == 0) {
            return loc;
        }
        loc++;
        if (loc >= MX) {
            loc %= MX;
        }
        if (loc == ori) {
            return -1;
        }
    }
    return -1;
}

static int hash_insert(const char *name) {
    int len = strlen(name);
    int index = hash_locate(name);
    if (index < 0 || len < 0 || table[index]) {
        return -1;
    }
    table[index] = (char *)kmalloc(len + 1);
    memcpy(table[index], name, len + 1);
    return index;
}

static int hash_remove(const char *name) {
    int index = hash_find(name);
    if (index == -1 || !table[index] || strcmp(name, table[index])) {
        return -1;
    }
    table[index] = NULL;
    return index;
}

int add_module(const char *name, struct elf_mod_info_s * info) {
    int ret = hash_insert(name);
    if (ret >= 0) {
        modules[ret] = info;
    }
    return ret;
}

int del_module(const char *name) {
    int ret = hash_remove(name);
    if (ret >= 0) {
        modules[ret] = NULL;
    }
    return ret;
}

struct elf_mod_info_s * get_module(const char *name) {
    int ret = hash_find(name);
    if (ret >= 0) {
        return modules[ret];
    }
    return NULL;
}

/**
 *  return whether a module has been loaded
 *  return 1 means loaded, 0 means not loaded
 */
int module_loaded(const char *name) {
    return hash_find(name) >= 0;
}

void print_loaded_module() {
    output("module        size\n");
    int i;
    for (i = 0; i < MX; i++) {
        if (table[i]) {
            output("%s        %d\n", table[i], modules[i]->image_size);
        }
    }
}

