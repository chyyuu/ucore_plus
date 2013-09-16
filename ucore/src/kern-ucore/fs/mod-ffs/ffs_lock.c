#include <types.h>
#include <sem.h>
#include "ffs.h"

void lock_ffs_fs(struct ffs_fs *ffs)
{
	//down(&(ffs->fs_sem));
}

void lock_ffs_io(struct ffs_fs *ffs)
{
	//down(&(ffs->io_sem));
}

void lock_ffs_mutex(struct ffs_fs *ffs)
{
	//down(&(ffs->mutex_sem));
}

void unlock_ffs_fs(struct ffs_fs *ffs)
{
	//up(&(ffs->fs_sem));
}

void unlock_ffs_io(struct ffs_fs *ffs)
{
	//up(&(ffs->io_sem));
}

void unlock_ffs_mutex(struct ffs_fs *ffs)
{
	//up(&(ffs->mutex_sem));
}
