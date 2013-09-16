#include <types.h>
#include <error.h>
#include <assert.h>
#include "ffconf.h"
#include "ffs.h"

#include <kio.h>
#include <mod.h>
#include <vfs.h>

static __init int ffs_init()
{
    int ret;
	if ((ret = register_filesystem("ffs", ffs_mount)) != 0) {
		panic("failed: ffs: register_filesystem: %e.\n", ret);
		return(-1);
	}
    kprintf("ffs_init: Hello world!\n");
	return(0);
}

static __exit void ffs_exit() {
    int ret;
	if ((ret = unregister_filesystem("ffs")) != 0) {
		panic("failed: ffs: unregister_filesystem: %e.\n", ret);
		return(-1);
	}
	kprintf("ffs_exit: Goodbye, cruel world.\n");
	return(0);
}

module_init(ffs_init);
module_exit(ffs_exit);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "mod-ffs",
    .init = ffs_init,
    .exit = ffs_exit,
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
	"depends=";

DWORD get_fattime(void)
{
	return ((DWORD) (2011 - 1980) << 25)
	    | ((DWORD) 3 << 21)
	    | ((DWORD) 26 << 16)
	    | ((DWORD) 19 << 11)
	    | ((DWORD) 28 << 5)
	    | ((DWORD) 0 << 1);
}

#if _FS_REENTRANT

bool ff_cre_syncobj(BYTE _vol, _SYNC_t * sobj)
{
	bool ret = false;
	return ret;
}

bool ff_del_syncobj(_SYNC_t sobj)
{
	bool ret = false;
	return ret;
}

bool ff_req_grant(_SYNC_t sobj)
{
	bool ret = false;
	return ret;
}

void ff_rel_grant(_SYNC_t sobj)
#endif
#if _USE_LFN == 3
void *ff_memalloc(UINT size)
{
	return malloc(size);
}

void ff_memfree(void *mblock)
{
	free(mblock);
}
#endif
