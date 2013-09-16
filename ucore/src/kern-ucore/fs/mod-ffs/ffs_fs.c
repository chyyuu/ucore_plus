#include <types.h>
#include <stdio.h>
#include <string.h>
#include <slab.h>
#include <list.h>
#include <fs.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>
#include "ffs.h"
#include "ff.h"

/* 
 * flush all dirty buffers to disk
 * return 0 if sync successful
 */
static int ffs_sync(struct fs *fs)
{
	//TODO
	return 0;
	FAT_PRINTF("[ffs_sync]\n");
	struct ffs_fs *ffs = fsop_info(fs, ffs);
	struct ffs_inode_list *inode_list = ffs->inode_list;
	while (inode_list->next != NULL) {
		inode_list = inode_list->next;
		vop_fsync(info2node(inode_list->f_inode, ffs_inode));
	}

	return 0;
}

/* return root inode of filesystem */
static struct inode *ffs_get_root(struct fs *fs)
{
	struct inode *node;
	int ret;
	if ((ret = ffs_load_inode(fsop_info(fs, ffs), &node, "0:/", NULL)) != 0) {
		panic("load ffs root failed: %e", ret);
	}

	return node;
}

/* attempt unmount of filesystem */
static int ffs_unmount(struct fs *fs)
{
	//TODO
	FAT_PRINTF("[ffs_unmount]\n");
	struct ffs_fs *ffs = fsop_info(fs, ffs);
	if (ffs->inode_list->next != NULL) {
		return -E_BUSY;
	}
	kfree(ffs->fatfs);
	kfree(ffs->inode_list);
	kfree(ffs);

	return 0;
}

/* cleanup of filesystem
 * i.e. sync the filesystem
 */
static int ffs_cleanup(struct fs *fs)
{
	FAT_PRINTF("[ffs_cleanup]");
	int i, ret;
	//just try 32 times
	for (i = 0; i < 32; i++) {
		if ((ret = fsop_sync(fs)) == 0) {
			break;
		}
	}
	if (ret != 0) {
		warn("ffs: sync error: %e.\n", ret);
	}
}

static int ffs_do_mount(struct device *dev, struct fs **fs_store)
{
	static_assert(FFS_BLKSIZE >= sizeof(struct ffs_disk_inode));

	if (dev->d_blocksize != FFS_BLKSIZE) {
		return -E_NA_DEV;
	}

	/* allocate fs structure */
	struct fs *fs;

	//alloc_fs(type) will distribute the memory and set the type to "type"
	if ((fs = alloc_fs(ffs)) == NULL) {
		return -E_NO_MEM;
	}
	struct ffs_fs *ffs = fsop_info(fs, ffs);

	FRESULT result;
	struct FATFS *fatfs = kmalloc(FFS_BLKSIZE);
	if ((result = f_mount(0, fatfs)) != FR_OK) {
		FAT_PRINTF("[ffs_do_mount], failed = %d\n", result);
		goto failed_cleanup_ffs;
	}
	ffs->fatfs = fatfs;

	/***********************/
	/* read dir test */
	/*
	   DIR dirobject;
	   FILINFO fno;

	   if ((result = f_opendir(&dirobject, "0:")) != 0) {
	   FAT_PRINTF("ls: opendir failed, %d.\n", result);
	   goto label_out;
	   }
	   while (1) {
	   if (f_readdir(&dirobject, &fno) != 0) {
	   FAT_PRINTF("ls: readdir failed.\n");
	   break;
	   }
	   if (strlen(fno.fname) < 1) break;
	   FAT_PRINTF("%s ", fno.fname);
	   }
	   label_out:
	   FAT_PRINTF("\n");
	 */
	/***********************/
	/* read file test */
	/*
	   struct FIL* fp;
	   fp = kmalloc(sizeof(struct FIL));
	   if ((result = f_open(fp, "ctest", FA_READ)) != FR_OK) {
	   FAT_PRINTF("[ffs_do_mount], f_open, %d\n", result);

	   } else {
	   FAT_PRINTF("f_open successful\n");
	   BYTE s[4096];
	   UINT rc = 0;
	   fp->fptr = 0;
	   if ((result = f_read(fp, s, 4096, &rc)) != FR_OK) {
	   FAT_PRINTF("!!!!!!!!!!!!!!!, result = %d\n", result);
	   }
	   else {
	   FAT_PRINTF("s = %s\n", s);
	   FAT_PRINTF("rc = %d", rc);
	   }
	   FAT_PRINTF("\n");
	   }
	   kfree(fp);
	 */
	/***********************/

	/* alloc and initialize inode_list */
	struct ffs_inode_list *head;
	if ((ffs->inode_list = head = kmalloc(FFS_BLKSIZE)) == NULL) {
		goto failed_cleanup_ffs;
	}
	ffs->inode_list->next = ffs->inode_list->prev = NULL;
	ffs->inocnt = 0;

	FAT_PRINTF("ffs_do_mount done\n");
	fs->fs_sync = ffs_sync;
	fs->fs_get_root = ffs_get_root;
	fs->fs_unmount = ffs_unmount;
	fs->fs_cleanup = ffs_cleanup;
	*fs_store = fs;
	return 0;

failed_cleanup_ffs:
	kfree(fatfs);
	kfree(fs);
	return -E_NO_MEM;
}

int ffs_mount(const char *devname)
{
	return vfs_mount(devname, ffs_do_mount);
}
