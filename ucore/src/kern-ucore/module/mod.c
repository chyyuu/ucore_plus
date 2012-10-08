#include <assert.h>
#include <string.h>
#include <kio.h>
#include <file.h>
#include <stat.h>
#include <slab.h>
#include <elf.h>
#include <bitsearch.h>
#include <mmu.h>
#include <vmm.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <ide.h>
#include <mod_loader.h>
#include <mod.h>
#include <mod_manager.h>
#include <sem.h>
#include <stdlib.h>
#include <error.h>

#define current (pls_read(current))

#define EXPORT(name) touch_export_sym(#name, (uintptr_t)&name, 0)
typedef void (*voidfunc)();

static unsigned char ko_pool[1000000];
static uintptr_t ko_pool_pointer;

unsigned char bss_pool[1000000];
uintptr_t bss_pool_ptr;

void register_mod_add(func_add_t f) {
    mod_touch_symbol(MOD_ADD, (void *)f, 0);
}

void unregister_mod_add() {
    mod_disable_symbol(MOD_ADD);
}

void register_mod_mul(func_mul_t f) {
    mod_touch_symbol(MOD_MUL, (void *)f, 0);
}

void unregister_mod_mul() {
    mod_disable_symbol(MOD_MUL);
}

int load_mod_file(char *name, uintptr_t *addr, uint32_t *size) {
    int fd = file_open(name, O_RDONLY);
    if (fd < 0) {
        error("cannot find kernel object file: %s\n", name);
        return -1;
    }
    struct stat mod_stat;
    memset(&mod_stat, 0, sizeof(mod_stat));
    file_fstat(fd, &mod_stat);
    if (mod_stat.st_size <= 0 || mod_stat.st_size > (1<<20)) {
        error("wrong obj file size: %d\n", mod_stat.st_size);
        return -1;
    }
    info("loading kern module: %s, size = %d\n", name, mod_stat.st_size);
    void * buffer = (void *)ko_pool_pointer;
    if (buffer == NULL) {
        error("not enough memory to load the module\n");
        return -1;
    }
    size_t copied;
    file_read(fd, buffer, mod_stat.st_size, &copied);
    info("[ II ] module size: %d, mem addr: %x\n", copied, buffer);
    ko_pool_pointer += copied;
    *addr = (uintptr_t)buffer;
    *size = mod_stat.st_size;
    return 0;
}

int load_module(char * name) {
    struct elf_mod_info_s *info = (struct elf_mod_info_s *)kmalloc(sizeof(struct elf_mod_info_s));
    add_module(name, info);    // must add module before load_mod_file, cuz load_mod_file will modify name

    int ret = 0;
    uintptr_t mod_addr;
    uint32_t mod_size;
    if ((ret = load_mod_file(name, &mod_addr, &mod_size)) != 0) {
        return ret;
    }

    elf_mod_load(mod_addr, mod_size, info);

    ((voidfunc)info->load_ptr)();
    return ret;
}

int unload_module(const char *name) {
    struct elf_mod_info_s *info = get_module(name);
    if (info == NULL) {
        error("module info not found for %s\n", name);
        return -1;
    }
    ((voidfunc)info->unload_ptr)();

    return del_module(name);
}

void mod_init() {
    kprintf("[ II ] mod_init\n");
    ko_pool_pointer = (uintptr_t)ko_pool;
    bss_pool_ptr = (uintptr_t)bss_pool;

    mod_loader_init();

    info("[ II ] exporting kernel symbols\n");

    EXPORT(kprintf);
    EXPORT(kfree);
    EXPORT(kmalloc);
    EXPORT(memcmp);
    EXPORT(memset);
    EXPORT(memcpy);
    EXPORT(strlen);
    EXPORT(strcmp);
    EXPORT(stricmp);
    EXPORT(strcpy);
    EXPORT(strchr);
    EXPORT(__panic);
    EXPORT(__warn);

    EXPORT(inode_init);
    EXPORT(inode_check);
    EXPORT(inode_ref_inc);
    EXPORT(inode_ref_dec);
    EXPORT(iobuf_init);
    EXPORT(iobuf_skip);
    EXPORT(iobuf_move);
    EXPORT(vfs_mount);
    EXPORT(__alloc_fs);
    EXPORT(__alloc_inode);

    EXPORT(null_vop_pass);
    EXPORT(null_vop_inval);
    EXPORT(null_vop_unimp);
    EXPORT(null_vop_isdir);
    EXPORT(null_vop_notdir);

    EXPORT(ide_read_secs);
    EXPORT(ide_write_secs);

    EXPORT(sem_init);
    EXPORT(up);
    EXPORT(down);

    EXPORT(register_mod_add);
    EXPORT(unregister_mod_add);
    EXPORT(register_mod_mul);
    EXPORT(unregister_mod_mul);

    EXPORT(register_filesystem);
    EXPORT(unregister_filesystem);

    EXPORT(hash32);
    EXPORT(hashstr);
    
    // TODO: read mod dep file
   
    kprintf("[ II ] mod_init done\n");
}

#define MX_MOD_PATH_LEN 1024
static char tmp_path[MX_MOD_PATH_LEN];

int do_init_module(const char *name) {

    if (module_loaded(name)) {
        kprintf("[ WW ] module %s already loaded\n", name);
        return -1;
    }
    int ret;

    struct mm_struct *mm = current->mm;
    lock_mm(mm);
    int size = strlen(name);
    if (!copy_from_user(mm, tmp_path, name, size, 1)) {
        unlock_mm(mm);
        return -E_INVAL;
    }
    unlock_mm(mm);
    tmp_path[size] = '\0';
    
    ret = load_module(tmp_path);
    return ret;
}

int do_cleanup_module(const char *name) {
    if (!module_loaded(name)) {
        kprintf("[ EE ] module %s not loaded\n", name);
        return -1;
    }
    int ret;

    struct mm_struct *mm = current->mm;
    lock_mm(mm);
    int size = strlen(name);
    if (!copy_from_user(mm, tmp_path, name, size, 1)) {
        unlock_mm(mm);
        return -E_INVAL;
    }
    unlock_mm(mm);
    tmp_path[size] = '\0';

    ret = unload_module(name);
    return ret;
}

int do_mod_add(int a, int b) {
    int c = 0;
    int idx = find_export_sym(MOD_ADD, 0);
    if (idx < 0 || !get_sym_ptr(idx)) {
        kprintf("[ EE ] module add not loaded into kernel\n");
        return 0;
    }
    ((func_add_t)get_sym_ptr(idx))(a, b, &c);
    return c;
}

int do_mod_mul(int a, int b) {
    int c = 0;
    int idx = find_export_sym(MOD_MUL, 0);
    if (idx < 0 || !get_sym_ptr(idx)) {
        kprintf("[ EE ] module mul not loaded into kernel\n");
        return 0;
    }
    ((func_mul_t)get_sym_ptr(idx))(a, b, &c);
    return c;
}

