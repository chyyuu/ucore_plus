#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <types.h>
#include <mmu.h>
#include <sem.h>
#include <atomic.h>

#define SECTSIZE            512
#define PAGE_NSECT          (PGSIZE / SECTSIZE)

#define SWAP_DEV_NO         1
#define DISK0_DEV_NO        2
#define DISK2_DEV_NO        3

void fs_init(void);
void fs_cleanup(void);

struct inode;
struct file;

struct fs_struct {
	struct inode *pwd;
	struct file *filemap;
	atomic_t fs_count;
	semaphore_t fs_sem;
};

#define FS_STRUCT_BUFSIZE                       (2 * PGSIZE - sizeof(struct fs_struct))
#define FS_STRUCT_NENTRY                        (FS_STRUCT_BUFSIZE / sizeof(struct file))

void lock_fs(struct fs_struct *fs_struct);
void unlock_fs(struct fs_struct *fs_struct);

struct fs_struct *fs_create(void);
void fs_destroy(struct fs_struct *fs_struct);
void fs_closeall(struct fs_struct *fs_struct);
int dup_fs(struct fs_struct *to, struct fs_struct *from);

static inline int fs_count(struct fs_struct *fs_struct)
{
	return atomic_read(&(fs_struct->fs_count));
}

static inline int fs_count_inc(struct fs_struct *fs_struct)
{
	return atomic_add_return(&(fs_struct->fs_count), 1);
}

static inline int fs_count_dec(struct fs_struct *fs_struct)
{
	return atomic_sub_return(&(fs_struct->fs_count), 1);
}

#endif /* !__KERN_FS_FS_H__ */
