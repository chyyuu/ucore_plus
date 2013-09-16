#ifndef __KERN_FS_FATFS_FFS_H__
#define __KERN_FS_FATFS_FFS_H__

#include <types.h>
#include <mmu.h>
#include <list.h>
#include <sem.h>
#include <unistd.h>
#include "ff.h"

#define FFS_MAGIC			0x19ad6b2f	/* magic number for ffs */
#define FFS_BLKSIZE			PGSIZE	/* size of block */
#define FFS_NDIRECT			12	/* # of direct blocks in inode */
#define FFS_MAX_INFO_LEN	31	/* max length of information */
#define FFS_MAX_FNAME_LEN   FS_MAX_FNAME_LEN	/* max length of filename */
#define FFS_MAX_FILE_SIZE   (1024UL * 1024 * 64)	/* max file size (64M) */
#define FFS_BLKN_SUPER      0	/* block the superblock lives in */
#define FFS_BLKN_ROOT       1	/* location of the root dir inode */
#define FFS_BLKN_FREEMAP    2	/* 1st block of the freemap */

/* # of bits in a block */
#define FFS_BLKBITS                                 (FFS_BLKSIZE * CHAR_BIT)

/* # of entries in a block */
#define FFS_BLK_NENTRY                              (FFS_BLKSIZE / sizeof(uint32_t))

/* file types */
#define FFS_TYPE_INVAL                              0	/* Should not appear on disk */
#define FFS_TYPE_FILE                               1
#define FFS_TYPE_DIR                                2
#define FFS_TYPE_LINK                               3

/*
 * On-disk superblock
 * no use in fat32
 */
struct ffs_super {
	uint32_t magic;		/* magic number, should be FFS_MAGIC */
	uint32_t blocks;	/* # of blocks in fs */
	uint32_t unused_blocks;	/* # of unused blocks in fs */
	char info[FFS_MAX_INFO_LEN + 1];	/* infomation for ffs  */
};

/* inode (on disk) */
struct ffs_disk_inode {
	uint16_t type;		/* one of SYS_TYPE_* above */
	FILINFO info;
	uint32_t size;
	uint32_t blocks;
	union {
		struct FIL *file;
		struct DIR *dir;
	} entity;
};

/* file entry (on disk) */
struct ffs_disk_entry {
	uint32_t ino;		/* inode number */
	char name[FFS_MAX_FNAME_LEN + 1];	/* file name */
};

#define ffs_dentry_size                             \
	sizeof(((struct ffs_disk_entry *)0)->name)

/* inode for ffs */
struct ffs_inode {
	struct ffs_disk_inode *din;	/* on-disk inode */
	uint32_t hashno;	/* hash number */
	TCHAR *path;		/* absolute path */
	struct ffs_inode *parent;	/* parent inode */
	bool dirty;		/* true if inode modified */
	int reclaim_count;	/* kill inode if it hits zero */
	semaphore_t sem;	/* semaphore for din */
	//list_entry_t inode_link;                        /* entry for linked-list in ffs_fs */
	//list_entry_t hash_link;                         /* entry for hash linked-list in ffs_fs */
	struct ffs_inode_list *inode_link;	/* entry for linked-list in ffs_fs */
};

struct ffs_inode_list {
	struct ffs_inode *f_inode;
	struct ffs_inode_list *prev;
	struct ffs_inode_list *next;
};

#define le2fin(le, member)                          \
	to_struct((le), struct ffs_inode, member)

/* filesystem for ffs */
struct ffs_fs {
	struct ffs_super super;
	struct device *dev;
	bool super_dirty;
	struct ffs_inode_list *inode_list;
	uint32_t inocnt;
	FATFS *fatfs;
};

#define FFS_HLIST_SHIFT	10
#define FFS_HLIST_SIZE	(1 << FFS_HLIST_SHIFT)

struct fs;
struct inode;

int ffs_mount(const char *devname);

void lock_ffs_fs(struct ffs_fs *ffs);
void lock_ffs_io(struct ffs_fs *ffs);
void lock_ffs_mutex(struct ffs_fs *ffs);
void unlock_ffs_fs(struct ffs_fs *ffs);
void unlock_ffs_io(struct ffs_fs *ffs);
void unlock_ffs_mutex(struct ffs_fs *ffs);

int ffs_rblock(struct ffs_fs *ffs, void *buf, uint32_t blkno, uint32_t nblks);
int ffs_wblock(struct ffs_fs *ffs, void *buf, uint32_t blkno, uint32_t nblks);
int ffs_rbuf(struct ffs_fs *ffs, void *buf, size_t len, uint32_t blkno,
	     off_t offset);
int ffs_wbuf(struct ffs_fs *ffs, void *buf, size_t len, uint32_t blkno,
	     off_t offset);
int ffs_sync_super(struct ffs_fs *ffs);
int ffs_sync_freemap(struct ffs_fs *ffs);
int ffs_clear_block(struct ffs_fs *ffs, uint32_t blkno, uint32_t nblks);

int ffs_load_inode(struct ffs_fs *ffs, struct inode **node_store, TCHAR * path,
		   struct ffs_inode *parent);

#endif //__KERN_FS_FATFS_FFS_H__
