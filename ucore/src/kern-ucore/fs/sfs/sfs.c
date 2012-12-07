#include <types.h>
#include <sfs.h>
#include <error.h>
#include <assert.h>
#include <vfs.h>

void
sfs_init(void) {
    int ret;
    if ((ret = register_filesystem("sfs", sfs_mount)) != 0) {
    	panic("failed: sfs: register_filesystem: %e.\n", ret);
    }
    if ((ret = sfs_mount("disk0")) != 0) {
        panic("failed: sfs: sfs_mount: %e.\n", ret);
    }
}

