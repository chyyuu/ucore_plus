#include <types.h>
#include <slab.h>
#include <list.h>
#include <sem.h>
#include <vfs.h>
#include <inode.h>
#include <pipe.h>
#include <error.h>
#include <assert.h>

void
lock_pipe(struct pipe_fs *pipe) {
    down(&(pipe->pipe_sem));
}

void
unlock_pipe(struct pipe_fs *pipe) {
    up(&(pipe->pipe_sem));
}

static int
pipe_sync(struct fs *fs) {
    return 0;
}

static struct inode *
pipe_get_root(struct fs *fs) {
    struct pipe_fs *pipe = fsop_info(fs, pipe);
    vop_ref_inc(pipe->root);
    return pipe->root;
}

static int
pipe_unmount(struct fs *fs) {
    return -E_INVAL;
}

static int
pipe_cleanup(struct fs *fs) {
    /* do nothing */
    return 0;
}

static void
pipe_fs_init(struct fs *fs) {
    struct pipe_fs *pipe = fsop_info(fs, pipe);
    if ((pipe->root = pipe_create_root(fs)) == NULL) {
        panic("pipe: create root inode failed.\n");
    }
    sem_init(&(pipe->pipe_sem), 1);
    list_init(&(pipe->pipe_list));

    fs->fs_sync = pipe_sync;
    fs->fs_get_root = pipe_get_root;
    fs->fs_unmount = pipe_unmount;
    fs->fs_cleanup = pipe_cleanup;
}

void
pipe_init(void) {
    struct fs *fs;
    if ((fs = alloc_fs(pipe)) == NULL) {
        panic("pipe: create pipe_fs failed.\n");
    }
    pipe_fs_init(fs);

    int ret;
    if ((ret = vfs_add_fs("pipe", fs)) != 0) {
        panic("pipe: vfs_add_fs: %e.\n", ret);
    }
}

