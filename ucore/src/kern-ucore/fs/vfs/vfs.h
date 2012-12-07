#ifndef __KERN_FS_VFS_VFS_H__
#define __KERN_FS_VFS_VFS_H__

#include <types.h>
#include <fs.h>
#include <pipe.h>
#include <sfs.h>
#include <fatfs/ffs.h>
#include <yaffs2_direct/yaffs_vfs.h>

struct inode;   // abstract structure for an on-disk file (inode.h)
struct device;  // abstract structure for a device (dev.h)
struct iobuf;   // kernel or userspace I/O buffer (iobuf.h)

/*
 * Abstract filesystem. (Or device accessible as a file.)
 *
 * Information:
 *      fs_info   : filesystem-specific data (pipe_fs/sfs_fs)
 *      fs_type   : filesystem type
 * Operations:
 *
 *      fs_sync       - Flush all dirty buffers to disk.
 *      fs_get_root   - Return root inode of filesystem.
 *      fs_unmount    - Attempt unmount of filesystem.
 *      fs_cleanup    - Cleanup of filesystem.???
 *      
 *
 * fs_get_root should increment the refcount of the inode returned.
 * It should not ever return NULL.
 *
 * If fs_unmount returns an error, the filesystem stays mounted, and
 * consequently the struct fs instance should remain valid. On success,
 * however, the filesystem object and all storage associated with the
 * filesystem should have been discarded/released.
 *
 */
struct fs {
    union {
        struct pipe_fs __pipe_info;
        struct sfs_fs __sfs_info;
#ifdef UCONFIG_HAVE_YAFFS2
        struct yaffs2_fs __yaffs2_info;
#endif
#ifdef UCONFIG_HAVE_FATFS
        struct ffs_fs __ffs_info;
#endif
    } fs_info;
    enum {
      fs_type_pipe_info = 0x5678,
      fs_type_sfs_info,
#ifdef UCONFIG_HAVE_YAFFS2
      fs_type_yaffs2_info,
#endif
#ifdef UCONFIG_HAVE_FATFS
      fs_type_ffs_info,
#endif
    } fs_type;
    int (*fs_sync)(struct fs *fs);
    struct inode *(*fs_get_root)(struct fs *fs);
    int (*fs_unmount)(struct fs *fs);
    int (*fs_cleanup)(struct fs *fs);
};

struct file_system_type
{
    const char *name;
    int (*mount)(const char *devname);
    list_entry_t file_system_type_link;
};


#define __fs_type(type)                                             fs_type_##type##_info

#define check_fs_type(fs, type)                                     ((fs)->fs_type == __fs_type(type))

#define __fsop_info(_fs, type) ({                                   \
            struct fs *__fs = (_fs);                                \
            assert(__fs != NULL && check_fs_type(__fs, type));      \
            &(__fs->fs_info.__##type##_info);                       \
        })

#define fsop_info(fs, type)                 __fsop_info(fs, type)

#define info2fs(info, type)                                         \
    to_struct((info), struct fs, fs_info.__##type##_info)

struct fs *__alloc_fs(int type);

#define alloc_fs(type)                                              __alloc_fs(__fs_type(type))

// Macros to shorten the calling sequences.
#define fsop_sync(fs)                       ((fs)->fs_sync(fs))
#define fsop_get_root(fs)                   ((fs)->fs_get_root(fs))
#define fsop_unmount(fs)                    ((fs)->fs_unmount(fs))
#define fsop_cleanup(fs)                    ((fs)->fs_cleanup(fs))

/*
 * Virtual File System layer functions.
 *
 * The VFS layer translates operations on abstract on-disk files or
 * pathnames to operations on specific files on specific filesystems.
 */
void vfs_init(void);
void vfs_cleanup(void);
void vfs_devlist_init(void);

/*
 * VFS layer low-level operations. 
 * See inode.h for direct operations on inodes.
 * See fs.h for direct operations on filesystems/devices.
 *
 *    vfs_set_curdir   - change current directory of current thread by inode
 *    vfs_get_curdir   - retrieve inode of current directory of current thread
 *    vfs_sync         - force all dirty buffers to disk
 *    vfs_get_root     - get root inode for the filesystem named DEVNAME
 *    vfs_get_devname  - get mounted device name for the filesystem passed in
 */
int vfs_set_curdir(struct inode *dir);
int vfs_get_curdir(struct inode **dir_store);
int vfs_sync(void);
int vfs_get_root(const char *devname, struct inode **root_store);
const char *vfs_get_devname(struct fs *fs);


/*
 * VFS layer high-level operations on pathnames
 * Because namei may destroy pathnames, these all may too.
 *
 *    vfs_open         - Open or create a file. FLAGS/MODE per the syscall. 
 *    vfs_close  - Close a inode opened with vfs_open. Does not fail.
 *                 (See vfspath.c for a discussion of why.)
 *    vfs_link         - Create a hard link to a file.
 *    vfs_symlink      - Create a symlink PATH containing contents CONTENTS.
 *    vfs_readlink     - Read contents of a symlink into a uio.
 *    vfs_mkdir        - Create a directory. MODE per the syscall.
 *    vfs_unlink       - Delete a file/directory.
 *    vfs_rename       - rename a file.
 *    vfs_chdir  - Change current directory of current thread by name.
 *    vfs_getcwd - Retrieve name of current directory of current thread.
 *
 */
int vfs_open(char *path, uint32_t open_flags, struct inode **inode_store);
int vfs_close(struct inode *node);
int vfs_link(char *old_path, char *new_path);
int vfs_symlink(char *old_path, char *new_path);
int vfs_readlink(char *path, struct iobuf *iob);
int vfs_mkdir(char *path);
int vfs_unlink(char *path);
int vfs_rename(char *old_path, char *new_path);
int vfs_chdir(char *path);
int vfs_getcwd(struct iobuf *iob);


/*
 * VFS layer mid-level operations.
 *
 *    vfs_lookup     - Like VOP_LOOKUP, but takes a full device:path name,
 *                     or a name relative to the current directory, and
 *                     goes to the correct filesystem.
 *    vfs_lookparent - Likewise, for VOP_LOOKPARENT.
 *
 * Both of these may destroy the path passed in.
 */
int vfs_lookup(char *path, struct inode **node_store);
int vfs_lookup_parent(char *path, struct inode **node_store, char **endp);

/*
 * Misc
 *
 *    vfs_set_bootfs - Set the filesystem that paths beginning with a
 *                    slash are sent to. If not set, these paths fail
 *                    with ENOENT. The argument should be the device
 *                    name or volume name for the filesystem (such as
 *                    "lhd0:") but need not have the trailing colon.
 *
 *    vfs_get_bootfs - return the inode of the bootfs filesystem. 
 *
 *    vfs_add_fs     - Add a hardwired filesystem to the VFS named device
 *                    list. It will be accessible as "devname:". This is
 *                    intended for filesystem-devices like emufs, and
 *                    gizmos like Linux procfs or BSD kernfs, not for
 *                    mounting filesystems on disk devices.
 *
 *    vfs_add_dev    - Add a device to the VFS named device list. If
 *                    MOUNTABLE is zero, the device will be accessible
 *                    as "DEVNAME:". If the mountable flag is set, the
 *                    device will be accessible as "DEVNAMEraw:" and
 *                    mountable under the name "DEVNAME". Thus, the
 *                    console, added with MOUNTABLE not set, would be
 *                    accessed by pathname as "con:", and lhd0, added
 *                    with mountable set, would be accessed by
 *                    pathname as "lhd0raw:" and mounted by passing
 *                    "lhd0" to vfs_mount.
 *
 *    vfs_mount      - Attempt to mount a filesystem on a device. The
 *                    device named by DEVNAME will be looked up and 
 *                    passed, along with DATA, to the supplied function
 *                    MOUNTFUNC, which should create a struct fs and
 *                    return it in RESULT.
 *
 *    vfs_unmount    - Unmount the filesystem presently mounted on the
 *                    specified device.
 *
 *    vfs_unmountall - Unmount all mounted filesystems.
 */
int vfs_set_bootfs(char *fsname);
int vfs_get_bootfs(struct inode **node_store);

int vfs_add_fs(const char *devname, struct fs *fs);
int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable);

int vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store));
int vfs_unmount(const char *devname);
int vfs_unmount_all(void);

void file_system_type_list_init(void);
int register_filesystem(const char *name, int (*mount)(const char *devname));
int unregister_filesystem(const char *name);

int do_mount(const char *devname, const char *fsname);
int do_umount(const char *devname);

#endif /* !__KERN_FS_VFS_VFS_H__ */

