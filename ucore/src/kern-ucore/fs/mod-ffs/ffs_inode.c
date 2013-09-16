#include <kdebug.h>
#include <stdio.h>
#include <types.h>
#include <string.h>
#include <stdlib.h>
#include <slab.h>
#include <list.h>
#include <stat.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>
#include "ffs.h"

static const struct inode_ops ffs_node_dirops;
static const struct inode_ops ffs_node_fileops;

/* *
 *  ffs_get_ops:    return corresponding inode_ops according to the type
 *  @type:  the data type, dir of file.
 */
static const struct inode_ops *ffs_get_ops(uint16_t type)
{
	switch (type) {
	case FFS_TYPE_DIR:
		return &ffs_node_dirops;
	case FFS_TYPE_FILE:
		return &ffs_node_fileops;
	}
	panic("invalid file type %d.\n", type);
}

static uint32_t hashstr(char *name)
{
	uint32_t hash = 47;
	int i;
	for (i = 0; i < 8 && name[i] != '\0'; ++i) {
		hash = hash * 31 + name[i];
	}
	return hash;
}

/* *
 *  hash:   compute a hash value of an absolute path.
 *  @path:  the absolute path to be computed the hash value.
 */
static uint32_t hash(TCHAR * path)
{
	return hash32(hashstr(path), FFS_HLIST_SHIFT);
}

/* *
 *  getAbsolutePath:    compute absolute path according to father inode and relative path.
 *  @fin:   the father inode.
 *  @path:  the relative path according to fin.
 */
static char *getAbsolutePath(struct ffs_inode *fin, char *path)
{
#if PRINTFSINFO
	FAT_PRINTF("[getAbsolutePath]\n");
#endif
	int len = strlen(path);
	struct ffs_inode *_fin = fin;
	if (_fin != NULL && strcmp("0:/", _fin->path)) {
		len += strlen(_fin->path);
		/* for '/' in path */
		++len;
		_fin = _fin->parent;
	}
	char *ret = kmalloc(sizeof(char) * (len + 1));
	len -= strlen(path);
	_fin = fin;
	char *p;
	strcpy(ret + len, path);
	int tlen, i;
	if (_fin != NULL && strcmp("0:/", _fin->path)) {
		p = _fin->path;
		tlen = strlen(_fin->path);
		len = len - tlen - 1;
		assert(len >= 0);
		for (i = 0; i < tlen; ++i) {
			*(ret + len + i) = *(p + i);
		}
		*(ret + len + tlen) = '/';
		_fin = _fin->parent;
	}
#if PRINTFSINFO
	FAT_PRINTF("[getAbsolutePath] path = %s\n", ret);
#endif
	return ret;
}

static char *ffs_lookup_subpath(char *path)
{
	if ((path = strchr(path, '/')) != NULL) {
		while (*path == '/') {
			*path++ = '\0';
		}
		if (*path == '\0') {
			return NULL;
		}
	}
	return path;
}

/*  ffs_dirent_search_nolock:    search if name in fin.
 *  @fin:   the father inode.
 *  @name:  an entry name in fin. There will be no '/' in name.
 */
static int ffs_dirent_search_nolock(struct ffs_inode *fin, const char *name)
{
	int ret = -E_NOENT;
	DIR dirobject;
	FILINFO fno;
	int result;
	//dirobject = kmalloc(sizeof(struct FIL));
	if ((result = f_opendir(&dirobject, fin->path)) != 0) {
		FAT_PRINTF("ls: opendir failed, %d.\n", result);
		return ret;
	}
	while (1) {
		if (f_readdir(&dirobject, &fno) < 0) {
			FAT_PRINTF("ls: readdir failed.\n");
			break;
		}
		if (strlen(fno.fname) < 1)
			break;
		if (!stricmp(fno.fname, name)) {
			return 0;
		}
	}
	return ret;
}

/*  ffs_lookup_once: if name in fin, store the inode into node_store.
 *  @ffs:   the fat32 file system.
 *  @fin:   the father inode.
 *  @name:  the entry name in fin. There will be no '/' in the name.
 *  @node_store:    if right inode found, *node_store will be assigned it, else ret != 0.
 */
static int
ffs_lookup_once(struct ffs_fs *ffs, struct ffs_inode *fin, TCHAR * name,
		struct inode **node_store)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_lookup_once] path = %s, name = %s\n", fin->path, name);
#endif
	int ret = -E_NOENT;
	if (!strcmp(name, ".")) {
		ret = 0;
		*node_store = info2node(fin, ffs_inode);
		vop_ref_inc(*node_store);
		goto label_out;
	}
	if (!strcmp(name, "..")) {
		ret = 0;
		if (fin->parent == NULL || !strcmp("0:/", fin->path)) {
			/* if it's root */
			*node_store = info2node(fin, ffs_inode);
			vop_ref_inc(*node_store);
			goto label_out;
		} else {
			ret =
			    ffs_load_inode(ffs, node_store, fin->parent->path,
					   fin->parent->parent);
			goto label_out;
		}
	}

	ret = ffs_dirent_search_nolock(fin, name);

	if (ret == 0) {
		ret = ffs_load_inode(ffs, node_store, name, fin);
	}
label_out:
	return ret;
}

/*  lookup_ffs_nolock:  search if there's an inode in ffs->inode_list according
 *                      whose information same as path.
 *  @ffs:   the fat32 file system.
 *  @path:  the absolute path to match.
 *  @parent:    the father's inode of the file or dir.
 */
static struct inode *lookup_ffs_nolock(struct ffs_fs *ffs, TCHAR * path,
				       struct ffs_inode *parent)
{
	//get the inode have been created in ffs whose path == path
	//add ffs_inode*(a list) to ffs_fs, remove list_entry_t
	//add maintain the node_link (type of ffs_inode*), pointing to the corresponding entry in ffs_fs->ffs_inode
	//change the ino to a hash value of path, for faster searching
#if PRINTFSINFO
	FAT_PRINTF("[lookup_ffs_nolock] path = %s\n", path);
#endif
	struct inode *node;
	uint32_t hashValue = hash(path);
	struct ffs_inode_list *head = ffs->inode_list;
	while (head->next != NULL) {
		head = head->next;
		struct ffs_inode *fin = head->f_inode;
		if (fin->hashno == hashValue && !strcmp(fin->path, path)) {
#if PRINTFSINFO
			FAT_PRINTF
			    ("[lookup_ffs_nolock] find defined node, path = %s, value = %d\n",
			     path, hashValue);
#endif
			node = info2node(fin, ffs_inode);
			if (vop_ref_inc(node) == 1) {
				fin->reclaim_count++;
			}
			return node;
		}
	}
	return NULL;
}

/* ffs_create_inode:    create a new ffs_inode according to path and store it into node_store.
 * @ffs:    the fat32 file system.
 * @din:    the ffs_disk_inode of the new ffs_inode.
 * @path:   relative path to the parent.
 * @node_store: *node_store will be assigned the new node.
 * @parent: the parent ffs_inode of the file of dir.
 */
static int
ffs_create_inode(struct ffs_fs *ffs, struct ffs_disk_inode *din,
		 const char *path, struct inode **node_store,
		 struct ffs_inode *parent)
{
	struct inode *node;
	if ((node = alloc_inode(ffs_inode)) != NULL) {
		vop_init(node, ffs_get_ops(din->type), info2fs(ffs, ffs));
		struct ffs_inode *fin = vop_info(node, ffs_inode);
		fin->din = din;
		fin->dirty = 0;
		fin->reclaim_count = 1;
		char *absPath = getAbsolutePath(parent, (char *)path);
		fin->path = absPath;
		fin->hashno = hash(absPath);
		fin->parent = parent;
		vop_ref_inc(node);
		*node_store = node;
		return 0;
	}

	return -E_NO_MEM;
}

/*  ffs_set_links:  add an new ffs_inode to ffs->inode_list.
 *  @ffs:   the fat32 file system.
 *  @fin:   the ffs_inode to be add.
 */
static void ffs_set_links(struct ffs_fs *ffs, struct ffs_inode *fin)
{
	struct ffs_inode_list *head = ffs->inode_list;
	struct ffs_inode_list *newEntry = NULL;
	if ((newEntry = kmalloc(sizeof(struct ffs_inode_list))) == NULL) {
		panic("[ffs_set_links] no room for newEntry");
	}
	while (head->next != NULL)
		head = head->next;
	newEntry->f_inode = fin;
	newEntry->prev = head;
	newEntry->next = NULL;
	head->next = newEntry;
	fin->inode_link = newEntry;
	++ffs->inocnt;
}

/*  ffs_remove_links:   remove an ffs_inode from ffs->inode_list.
 *  @ffs:   the fat32 file system.
 *  @fin:   the ffs_inode to be removed.
 */
static void ffs_remove_links(struct ffs_fs *ffs, struct ffs_inode *fin)
{
	struct ffs_inode_list *prev = fin->inode_link->prev;
	struct ffs_inode_list *next = fin->inode_link->next;
	if (prev != NULL)
		prev->next = next;
	if (next != NULL)
		next->prev = prev;
	kfree(fin->inode_link);
	//kfree(fin->path);
	//kfree(fin);
	--ffs->inocnt;
}

/*  ffs_load_inode: get an inode according to relative path and the parent and store it into node_store.
 *  firstly to search if there also is one in ffs->inode_list.
 *  if can't find, create a new inode and add it to the list.
 *  @ffs:   the fat32 file system.
 *  @node_store:    *node_store will store the got inode.
 */
int
ffs_load_inode(struct ffs_fs *ffs, struct inode **node_store, TCHAR * path,
	       struct ffs_inode *parent)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_load_inode] path = %s\n", path);
#endif
	int ret = -E_NO_MEM;
	struct inode *node;

	/* try to find in inode->list */
	if ((node = lookup_ffs_nolock(ffs, path, parent)) != NULL) {
		*node_store = node;
		return 0;
	}

	/* if not find */
	struct ffs_disk_inode *din;
	if ((din = kmalloc(sizeof(struct ffs_disk_inode))) == NULL) {
		panic("no room in ffs_load_inode first if");
		goto failed;
	}
	struct FILINFO fno;
	FRESULT result;

	if (strcmp(path, "0:/")) {
		/* if not root inode */
		char *actualPath = NULL;
		char *absPath;
		/* chrinstr */
		if (strchr(path, '/')) {
			actualPath = path;
		} else {
			absPath = getAbsolutePath(parent, path);
			actualPath = absPath;
		}

		if ((result = f_stat(actualPath, &fno)) != FR_OK) {
			FAT_PRINTF
			    ("[ffs_load_inode] get stat failed, f_stat %s %d\n",
			     absPath, result);
			kfree(absPath);
			goto failed_cleanup_din;
			return ret;
		} else {
#if PRINTFSINFO
			FAT_PRINTF
			    ("[ffs_load_inode] get stat success, f_stat %s",
			     absPath);
#endif
		}
		kfree(absPath);

		/* decide the type */
		if (!(fno.fattrib & AM_DIR)) {
			din->type = FFS_TYPE_FILE;
#if PRINTFSINFO
			FAT_PRINTF(", a file\n");
#endif
		} else {
			din->type = FFS_TYPE_DIR;
#if PRINTFSINFO
			FAT_PRINTF(", a dir\n");
#endif
		}

	} else {
		/* root inode */
		din->type = FFS_TYPE_DIR;
	}
	if (ffs_create_inode(ffs, din, path, &node, parent) != 0) {
		ret = -E_NO_MEM;
		goto failed_cleanup_din;
	}
	ffs_set_links(ffs, vop_info(node, ffs_inode));

	*node_store = node;
	return 0;

failed_cleanup_din:
	kfree(din);
failed:
	return ret;
}

/* ffs_opendir: open dir according to open_flags.
 * because of we can open dir when necessary with FatFs,
 * there's no need to realize the function, so return 0 directly here.
 * May there will be other requirement in the future, so I leave the
 * realization of mine here.
 */
static int ffs_opendir(struct inode *node, uint32_t open_flags)
{
	return 0;

	FAT_PRINTF("[ffs_opendir]\n");

	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FAT_PRINTF("[ffs_opendir], path = %s\n", fin->path);
	int ret = 0;
	fin->din->entity.dir = kmalloc(sizeof(struct DIR));
	if ((ret = f_opendir(fin->din->entity.dir, fin->path)) != FR_OK) {
		FAT_PRINTF("[ffs_opendir] failed, ret = %d\n", ret);
		return -E_TIMEOUT;
	}
	return ret;

}

/*  ffs_openfile:   open file according to open_flags and
 *  store the information into node.
 *  @node:  the corresponding file to be opened.
 *  @open_flags:    the open mode. O_RDONLY means read only,
 *  else will have the write to create and write the file.
 */
static int ffs_openfile(struct inode *node, uint32_t open_flags)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_openfile], open_flags = %d\n", open_flags);
#endif
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	BYTE mode;
	mode = FA_READ | FA_WRITE;

	switch (open_flags & O_ACCMODE) {
	case O_RDONLY:
		mode = FA_READ;
		break;
	default:
		mode = FA_READ | FA_CREATE_ALWAYS | FA_WRITE;
		break;
	}

	int ret;
	if ((fin->din->entity.file = kmalloc(sizeof(struct FIL))) == NULL) {
#if PRINTFSINFO
		FAT_PRINTF("[ffs_openfile], no memory for file\n");
#endif
		return -E_NO_MEM;
	}
	if ((ret = f_open(fin->din->entity.file, fin->path, mode)) != FR_OK) {
#if PRINTFSINFO
		FAT_PRINTF("[ffs_openfile], fin->path = %s, result = %d\n",
			   fin->path, ret);
#endif
		return -E_INVAL;
	}
	fin->din->size = fin->din->entity.file->fsize;
#if PRINTFSINFO
	FAT_PRINTF("[ffs_openfile], open %s success, mode = %d\n", fin->path,
		   mode);
#endif
	return 0;
}

/*
 *  ffs_close:  close the file and free the memory.
 *  @node:  the file to be closed.
 */
static int ffs_close(struct inode *node)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_close]\n");
#endif
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FRESULT result;
	if ((result = f_close(fin->din->entity.file)) != FR_OK) {
		FAT_PRINTF("[ffs_close] close %s failed for %d\n", fin->path,
			   result);
	}
	kfree(fin->din->entity.file);
	fin->din->entity.file = NULL;
	return vop_fsync(node);
}

/*
 *  ffs_closedir: close dir.
 *  Since ffs_opendir will do nothing, so will ffs_closedir.
 *  Both will be done when needed.
 */
static int ffs_closedir(struct inode *node)
{
	return 0;

	FAT_PRINTF("[ffs_closedir]\n");
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	kfree(fin->din->entity.dir);
	fin->din->entity.dir = NULL;
	return 0;
}

/*
 *  ffs_read:   read data in file and store it in iob.
 *  @node:  the file to be read.
 *  @iob:   io buffer to store the data.
 *  we use io_len stands for the bytes want to read and rc stands for the bytes actually read.
 *  after the read operation, io_base += rc, io_offset += rc, io_resid -= rc.
 */
static int ffs_read(struct inode *node, struct iobuf *iob)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_read]\n");
#endif
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FRESULT result;

#if PRINTFSINFO
	FAT_PRINTF("[ffs_read], io_offset = %d, io_len = %d, io_resid = %d\n",
		   iob->io_offset, iob->io_len, iob->io_resid);
#endif
	UINT rc = 0;
	BYTE buf[iob->io_len + 10];
	FIL *temp = fin->din->entity.file;
	/* move the start point to offset */
	f_lseek(temp, iob->io_offset);
	if ((result = f_read(temp, buf, iob->io_len, &rc)) != FR_OK) {
		FAT_PRINTF("[ffs_read], f_read failed, result = %d\n", result);
	} else {
		int i;
		assert(iob->io_len >= rc);
		for (i = 0; i < rc; ++i) {
			*((BYTE *) (iob->io_base)) = buf[i];
			iob->io_base = ((BYTE *) (iob->io_base)) + 1;
		}
		iob->io_offset += rc;
		iob->io_resid -= rc;
	}

	return 0;
}

/*
 *  ffs_read:   write data in iob into file.
 *  @node:  the file to be write.
 *  @iob:   io buffer where get the data.
 *  we use io_len stands for the bytes want to write and rc stands for the bytes actually write.
 *  after the write operation, io_base += rc, io_offset += rc, io_resid -= rc.
 */
static int ffs_write(struct inode *node, struct iobuf *iob)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_write]\n");
#endif
	int ret = -E_BUSY;
	UINT rc = 0;
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FIL *temp = fin->din->entity.file;
	/* move the start point to offset */
	f_lseek(temp, iob->io_offset);
	if ((ret = f_write(temp, iob->io_base, iob->io_len, &rc)) != FR_OK) {
		FAT_PRINTF("[ffs_write] result = %d\n", ret);
		return -E_BUSY;
	}
	assert(iob->io_len >= rc);
	iob->io_offset += rc;
	iob->io_base = ((BYTE *) iob->io_base) + rc;
	iob->io_resid -= rc;
	return ret;
}

/* 
 *  ffs_fstat:  get file or dir state, especially the type.
 *  @node:  the file or dir whose state will be got.
 *  @stat:  store the state information of the node.
 */
static int ffs_fstat(struct inode *node, struct stat *stat)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_fstat]\n");
#endif
	int ret = -E_INVAL;
	memset(stat, 0, sizeof(struct stat));

	if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) {
		return ret;
	}
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	stat->st_size = fin->din->size;
	return 0;
}

/*
 *  ffs_fsync:   synchronize the data of node into fat.img
 *  since FatFs module have done something to synchronize
 *  the data immediately, so this function is no use now.
 */
static int ffs_fsync(struct inode *node)
{
	return 0;

	FAT_PRINTF("[ffs_fsync]\n");
	int ret = 0;
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FRESULT result;
	if (fin->dirty) {
		fin->dirty = 0;
		if ((result = f_sync(fin->din->entity.file)) != FR_OK) {
			FAT_PRINTF("[ffs_fsync] sync %s failed for %d\n",
				   fin->path, result);
			fin->dirty = 1;
			ret = -E_INVAL;
		}
	}
	return ret;
}

/*
 *  ffs_dirent_create_inode:    create inode, used in mkdir and rename
 *  @ffs:   the fat32 file system.
 *  @fin:   the father inode.
 *  @type:  the type of the inode, FFS_TYPE_FILE or FFS_TYPE_DIR.
 *  @node_store:    *node_store will be assigned the new inode.
 *  @name:  the absolute path of the file or dir.
 */
static int
ffs_dirent_create_inode(struct ffs_fs *ffs, struct ffs_inode *fin,
			uint16_t type, struct inode **node_store,
			const char *name)
{
	struct ffs_disk_inode *din;
	if ((din = kmalloc(sizeof(struct ffs_disk_inode))) == NULL) {
		return -E_NO_MEM;
	}
	memset(din, 0, sizeof(struct ffs_disk_inode));
	din->type = type;

	int ret;

	struct inode *node;

	if ((ret = ffs_create_inode(ffs, din, name, &node, fin)) != 0) {
		goto failed_cleanup_din;
	}
	ffs_set_links(ffs, vop_info(node, ffs_inode));
	*node_store = node;
	return 0;

failed_cleanup_din:
	kfree(din);
	return ret;
}

/*
 *  ffs_mkdir:  make a new dir in node whose name is @name
 *  @name:  the name of the new dir.
 *  @node:  the parent dir of the new dir.
 */
static int ffs_mkdir(struct inode *node, const char *name)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_mkdir]\n");
#endif
	if (strlen(name) > FFS_MAX_FNAME_LEN) {
		return -E_TOO_BIG;
	}
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return -E_EXISTS;
	}

	int ret = -E_NO_MEM;
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	struct inode *link_node;
	if ((ret =
	     ffs_dirent_create_inode(ffs, fin, FFS_TYPE_DIR, &link_node,
				     name)) != 0) {
		return ret;
	}
	struct ffs_inode *lnkfin = vop_info(link_node, ffs_inode);
	if ((ret = f_mkdir(lnkfin->path)) != FR_OK) {
		FAT_PRINTF("[ffs_mkdir] error, result = %d\n", ret);
		return -E_NO_MEM;
	}
	vop_ref_dec(link_node);
	return ret;
}

/*
 *  ffs_link: since there's no hard link in fat32, 
 *  so this function is no use.
 */
static int
ffs_link(struct inode *node, const char *name, struct inode *link_node)
{
	FAT_PRINTF("[ffs_link]\n");
	if (strlen(name) > FFS_MAX_FNAME_LEN) {
		return -E_TOO_BIG;
	}
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return -E_EXISTS;
	}
	struct ffs_inode *lnkfin = vop_info(link_node, ffs_inode);
	if (lnkfin->din->type == FFS_TYPE_DIR) {
		return -E_ISDIR;
	}

	return -E_BUSY;

	int ret = -E_BUSY;

	struct ffs_inode *fin = vop_info(node, ffs_inode);
	if ((ret = ffs_dirent_search_nolock(fin, name)) != -E_NOENT) {
		return (ret != 0) ? ret : -E_EXISTS;
	}

	return ret;
}

/*
 *  findNode:   find the ffs_inode in ffs->inodelist according
 *  to hashno and absolute path.
 *  @ffs:   the fat32 file system.
 *  @name:  the absolute path of the file or dir.
 *  @hashno:    the hash value of the name.
 */
static struct ffs_inode *findNode(struct ffs_fs *ffs, const char *name,
				  uint32_t hashno)
{
	struct ffs_inode_list *head = ffs->inode_list;
	while (head->next != NULL) {
		head = head->next;
		struct ffs_inode *fin = head->f_inode;
		if (fin->hashno == hashno && !strcmp(fin->path, name)) {
			return fin;
		}
	}
	return NULL;
}

/* *
 *  ffs_rename: rename a dir or a file.
 *  @node:  the origin parent inode
 *  @name:  the origin relative path of the file or dir.
 *  @new_node:  the new parent inode.
 *  @new_name:  the new relative path of the file or dir.
 */
static int
ffs_rename(struct inode *node, const char *name, struct inode *new_node,
	   const char *new_name)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_rename]\n");
#endif
	if (strlen(name) > FFS_MAX_FNAME_LEN
	    || strlen(new_name) > FFS_MAX_FNAME_LEN) {
		return -E_TOO_BIG;
	}
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return -E_EXISTS;
	}
	if (!strcmp(new_name, ".") || !strcmp(new_name, "..")) {
		return -E_EXISTS;
	}

	int ret;
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	struct ffs_inode *fin = vop_info(node, ffs_inode), *newfin =
	    vop_info(new_node, ffs_inode);

	if ((ret = ffs_dirent_search_nolock(fin, name)) != 0) {
		return ret;
	}
	if ((ret = ffs_dirent_search_nolock(fin, new_name)) != -E_NOENT) {
		return (ret != 0) ? ret : -E_EXISTS;
	}

	char *absPath = getAbsolutePath(fin, (char *)name);
	char *newAbsPath = getAbsolutePath(newfin, (char *)new_name);
	f_rename(absPath, newAbsPath);

	struct ffs_inode *filnode = findNode(ffs, absPath, hash(absPath));
	if (filnode != NULL) {
		filnode->path = newAbsPath;
		filnode->hashno = hash(newAbsPath);
		fin->dirty = newfin->dirty = 1;

		if (fin != newfin) {
			filnode->parent = newfin;
		}
	}
	return 0;
}

/*
 *  ffs_namefile: get the absolute path of the node
 *  and store it into iob->io_base.
 *  @node:  the node whose absolute path is wanted.
 *  @iob:   io buffer to store the absolute path.
 *  After the operation need use iobuf_skip to maintain
 *  the information of the io buffer.
 */
static int ffs_namefile(struct inode *node, struct iobuf *iob)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_namefile]\n");
#endif
	if (iob->io_resid <= 2) {
		return -E_NO_MEM;
	}
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	size_t alen;

	alen = strlen(fin->path);
	vop_ref_inc(node);
	strcpy((char *)iob->io_base + 1, fin->path);

	if (fin->path[alen - 1] == '/' || fin->path[alen - 1] == '\\') {
		/* last char can not be '/' */
#if PRINTFSINFO
		FAT_PRINTF("[ffs_namefile], fin->path = %s\n", fin->path);
#endif
		--alen;
	} else {
		*((char *)iob->io_base) = '/';
	}
	iobuf_skip(iob, alen);
	return 0;
}

/*
 *  ffs_getdirentry:    get the entries in dir iteratively.
 *  according to the iob->offset and ffs_dentry_size to compute
 *  which entry of the dir is neeeded.
 *  store the name of the entry into iob->io_base and use iobuf_move
 *  to maintain iob. What's important here the third parameters of 
 *  iobuf_move must be ffs_dentry_size, else the kernel will terminate
 *  getting dir entry.
 *  @node:  the dir's node.
 *  @iob:   io buffer to store the entry's name.
 */
static int ffs_getdirentry(struct inode *node, struct iobuf *iob)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_getdirentry]\n");
#endif
	off_t offset = iob->io_offset;
	int slot;
	if (offset < 0 || offset % sfs_dentry_size != 0) {
		return -E_INVAL;
	}
	/* get which entry is needed */
	slot = offset / ffs_dentry_size;
	int cnt = 0;
	DIR dirobject;
	FILINFO fno;
	FRESULT result;
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	if ((result = f_opendir(&dirobject, fin->path)) != 0) {
		FAT_PRINTF("[ffs_getdirentry] opendir error = %d, path = %s\n",
			   result, fin->path);
		return -E_INVAL;
	}

	while (1) {
		if ((result = f_readdir(&dirobject, &fno)) != 0) {
			FAT_PRINTF("[ffs_getdirentry] readdir error = %d\n",
				   result);
			return -E_INVAL;
		}
		if (strlen(fno.fname) < 1) {
			/* read over */
			return -E_INVAL;
		}
		if (cnt == slot) {
			/* get the wanna one */
			result =
			    iobuf_move(iob, fno.fname, ffs_dentry_size, 1,
				       NULL);
			return result;
		}
		++cnt;
	}
	return 0;
}

/*
 *  ffs_reclaim:    delete the inode and free memory.
 *  since the parent inode is always needed, so the inode can
 *  not be deleted in fat32. As a result, this function is no
 *  use now.
 */
static int ffs_reclaim(struct inode *node)
{
	return 0;

	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	FAT_PRINTF("[ffs_reclaim], path = %s\n", fin->path);

	int ret = -E_BUSY;
	assert(fin->reclaim_count > 0);

	if ((--fin->reclaim_count) != 0 || inode_ref_count(node) != 0) {
		goto failed;
	}

	if (fin->dirty) {
		if ((ret = vop_fsync(node)) != 0) {
			FAT_PRINTF("[ffs_reclaim] sync failed\n");
			goto failed;
		}
	}
	ffs_remove_links(ffs, fin);

	kfree(fin->din);

	vop_kill(node);

	FAT_PRINTF("[ffs_reclaim] reclaim success\n");
	return 0;

failed:
	return ret;
}

/*
 *  ffs_gettype:    change the ffs type to stat type
 *  @node:  the file or dir to be got type.
 *  @type_store:    store the stat type.
 */
static int ffs_gettype(struct inode *node, uint32_t * type_store)
{

#if PRINTFSINFO
	FAT_PRINTF("[ffs_gettype]\n");
#endif

	struct ffs_disk_inode *din = vop_info(node, ffs_inode)->din;

	switch (din->type) {
	case FFS_TYPE_DIR:
#if PRINTFSINFO
		FAT_PRINTF("[ffs_gettype] FFS_TYPE_DIR\n");
#endif
		*type_store = S_IFDIR;
		return 0;
	case FFS_TYPE_FILE:
#if PRINTFSINFO
		FAT_PRINTF("[ffs_gettype] FFS_TYPE_FILE\n");
#endif
		*type_store = S_IFREG;
		return 0;
	case FFS_TYPE_LINK:
#if PRINTFSINFO
		FAT_PRINTF("[ffs_gettype] FFS_TYPE_LINK\n");
#endif
		*type_store = S_IFLNK;
		return 0;
	}
	panic("invalid file type %d.\n", din->type);
}

/*
 *  ffs_tryseek:    seek to the position of a file.
 *  @node:  the file to be seek.
 *  @pos:   position paramter.
 */
static int ffs_tryseek(struct inode *node, off_t pos)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_tryseek]\n");
#endif
	if (pos < 0 || pos >= FFS_MAX_FILE_SIZE) {
		return -E_INVAL;
	}
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	if (pos > fin->din->size) {
		FAT_PRINTF("[ffs_tryseek] too big pos: %d\n", pos);
		return vop_truncate(node, pos);
	}
	return 0;

}

/*
 *  ffs_truncdir:   truncate dir to needed size.
 *  Have no sample test data using this function.
 *  And there's no need to truncdir in fat32.
 */
static int ffs_truncdir(struct inode *node, off_t len)
{
	FAT_PRINTF("[ffs_truncdir]\n");
	int ret = -E_BUSY;
	return ret;
}

/*
 *  ffs_truncfile:   truncate file to needed size.
 *  Have no sample test data using this function.
 */

static int ffs_truncfile(struct inode *node, off_t len)
{
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	struct ffs_disk_inode *din = fin->din;

#if PRINTFSINFO
	FAT_PRINTF("[ffs_truncfile]\n");
	FAT_PRINTF("[ffs_truncfile] path = %s, len = %d\n", fin->path, len);
#endif
	if (len < 0 || len > FFS_MAX_FILE_SIZE) {
		return -E_INVAL;
	}
	return 0;

	int ret = -E_BUSY;
	f_lseek(din->entity.file, len);
	if ((ret = f_truncate(din->entity.file)) != FR_OK) {
		FAT_PRINTF("[ffs_truncfile], result = %d\n", ret);
		ret = -E_TOO_BIG;
	} else {
		ret = 0;
	}
	return ret;
}

/*
 *  ffs_create: create a regular file named @name in @node dir.
 *  @node:  the dir node
 *  @name:  the new file's name
 *  @excl:  if excl is nonzero, fail if the file already exists
 *  otherwise, use the existing file if there's one.
 *  @node_store:    store the new file's inode.
 */
static int
ffs_create(struct inode *node, const char *name, bool excl,
	   struct inode **node_store)
{
	if (strlen(name) > FFS_MAX_FNAME_LEN) {
		return -E_TOO_BIG;
	}
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return -E_EXISTS;
	}
#if PRINTFSINFO
	FAT_PRINTF("[ffs_create]\n");
#endif
	int ret;
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	struct inode *link_node;
	if ((ret = ffs_dirent_search_nolock(fin, name)) != -E_NOENT) {
		if (ret != 0) {
			return ret;
		}
		if (!excl) {
			if ((ret =
			     ffs_load_inode(ffs, &link_node, (char *)name,
					    fin)) != 0) {
				return ret;
			}
			if (vop_info(link_node, ffs_inode)->din->type ==
			    FFS_TYPE_FILE) {
				goto out;
			}
			vop_ref_dec(link_node);
		}
		return -E_EXISTS;
	} else {
		if ((ret =
		     ffs_dirent_create_inode(ffs, fin, FFS_TYPE_FILE,
					     &link_node, name)) != 0) {
			return ret;
		}

		struct ffs_inode *newfin = vop_info(link_node, ffs_inode);
		if ((newfin->din->entity.file =
		     kmalloc(sizeof(struct FIL))) == NULL) {
			return -E_NO_MEM;
		}
		f_open(newfin->din->entity.file, newfin->path,
		       FA_CREATE_ALWAYS | FA_WRITE);
		f_close(newfin->din->entity.file);
		newfin->din->entity.file = NULL;
	}

out:
	*node_store = link_node;
	return ret;
}

/*
 *  ffs_unlink: unlink, that is, delete the entry @name in @node.
 *  @node:  the dir's node.
 *  @name:  the entry's name to be unlink in node.
 */
static int ffs_unlink(struct inode *node, const char *name)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_unlink]\n");
#endif
	if (strlen(name) > FFS_MAX_FNAME_LEN) {

		return -E_TOO_BIG;
	}
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		return -E_ISDIR;
	}

	int ret = -E_BUSY;
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	struct ffs_inode *fin = vop_info(node, ffs_inode);
	if ((ret = ffs_dirent_search_nolock(fin, name)) != 0) {
		return ret;
	}
	struct inode *link_node;
	if ((ret = ffs_load_inode(ffs, &link_node, (char *)name, fin)) != 0) {
		return ret;
	}

	struct ffs_inode *lnkfin = vop_info(link_node, ffs_inode);

	if (lnkfin->dirty) {
		if ((ret = vop_fsync(link_node)) != 0) {
			FAT_PRINTF("[ffs_unlink] sync failed\n");
		}
	}
	ffs_remove_links(ffs, lnkfin);
	kfree(lnkfin->din);

	FRESULT result;
	if ((result = f_unlink(lnkfin->path)) != FR_OK) {
		FAT_PRINTF("[ffs_unlink] unlink failed, error = %d\n", result);
		ret = -E_UNIMP;
	} else {
		fin->dirty = 1;
		lnkfin->dirty = 1;
		ret = 0;
	}
	vop_ref_dec(link_node);
	return ret;
}

/*
 *  ffs_lookup: Look up the final inode recurivly.
 *  @node:  the node of the dir.
 *  @path:  relative path, may '/' in it, to the @node.
 *  @node_store:    *node_store will store the found node.
 */
static int ffs_lookup(struct inode *node, char *path, struct inode **node_store)
{
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	assert(*path != '\0' && *path != '/');
	vop_ref_inc(node);
#if PRINTFSINFO
	FAT_PRINTF("[ffs_lookup], node.path = %s, path = %s\n",
		   vop_info(node, ffs_inode)->path, path);
#endif

	do {
		struct ffs_inode *fin = vop_info(node, ffs_inode);
		if (fin->din->type != FFS_TYPE_DIR) {
			vop_ref_dec(node);
#if PRINTFSINFO
			FAT_PRINTF("[ffs_lookup] not dir\n");
#endif
			return -E_NOTDIR;
		}

		char *subpath = ffs_lookup_subpath(path);
#if PRINTFSINFO
		FAT_PRINTF("[ffs_lookup] get subpath=%s\n", subpath);
#endif
		if (strlen(path) > FFS_MAX_FNAME_LEN) {
			vop_ref_dec(node);
			return -E_TOO_BIG;
		}
		struct inode *subnode;
#if PRINTFSINFO
		FAT_PRINTF("[ffs_lookup] to lookup once, path=%s\n", path);
#endif
		int ret = ffs_lookup_once(ffs, fin, path, &subnode);
		vop_ref_dec(node);

		if (ret != 0) {
			return ret;
		}

		node = subnode, path = subpath;

	} while (path != NULL);
	*node_store = node;

	return 0;
}

/*
 *  ffs_lookup: Look up the final's parent inode recurivly.
 *  @node:  the node of the dir.
 *  @path:  relative path, may '/' in it, to the @node.
 *  @node_store:    *node_store will store the found node.
 */
static int
ffs_lookup_parent(struct inode *node, char *path, struct inode **node_store,
		  char **endp)
{
#if PRINTFSINFO
	FAT_PRINTF("[ffs_lookup_parent]\n");
#endif
	struct ffs_fs *ffs = fsop_info(vop_fs(node), ffs);
	assert(*path != '\0' && *path != '/');
	vop_ref_inc(node);
	while (1) {
		struct ffs_inode *fin = vop_info(node, ffs_inode);
		if (fin->din->type != FFS_TYPE_DIR) {
			vop_ref_dec(node);
			return -E_NOTDIR;
		}

		char *subpath = ffs_lookup_subpath(path);
		if (subpath == NULL) {
			*node_store = node, *endp = path;
			return 0;
		}

		if (strlen(path) > FFS_MAX_FNAME_LEN) {
			vop_ref_dec(node);
			return -E_TOO_BIG;
		}

		struct inode *subnode;
		int ret = ffs_lookup_once(ffs, fin, path, &subnode);

		vop_ref_dec(node);
		if (ret != 0) {
			return ret;
		}
		node = subnode, path = subpath;
	}
}

static const struct inode_ops ffs_node_dirops = {
	.vop_magic = VOP_MAGIC,
	.vop_open = ffs_opendir,
	.vop_close = ffs_closedir,
	.vop_read = NULL_VOP_ISDIR,
	.vop_write = NULL_VOP_ISDIR,
	.vop_fstat = ffs_fstat,
	.vop_fsync = ffs_fsync,
	.vop_mkdir = ffs_mkdir,
	//.vop_rmdir                                                = NULL_VOP_ISDIR,
	.vop_link = ffs_link,
	.vop_rename = ffs_rename,
	.vop_readlink = NULL_VOP_ISDIR,
	.vop_symlink = NULL_VOP_UNIMP,
	.vop_namefile = ffs_namefile,
	.vop_getdirentry = ffs_getdirentry,
	.vop_reclaim = ffs_reclaim,
	.vop_ioctl = NULL_VOP_INVAL,
	.vop_gettype = ffs_gettype,
	.vop_tryseek = NULL_VOP_ISDIR,
	.vop_truncate = ffs_truncdir,
	.vop_create = ffs_create,
	.vop_unlink = ffs_unlink,
	.vop_lookup = ffs_lookup,
	.vop_lookup_parent = ffs_lookup_parent,
};

static const struct inode_ops ffs_node_fileops = {
	.vop_magic = VOP_MAGIC,
	.vop_open = ffs_openfile,
	.vop_close = ffs_close,
	.vop_read = ffs_read,
	.vop_write = ffs_write,
	.vop_fstat = ffs_fstat,
	.vop_fsync = ffs_fsync,
	.vop_mkdir = NULL_VOP_NOTDIR,
	//.vop_rmdir                                            = NULL_VOP_NOTDIR,
	.vop_link = NULL_VOP_NOTDIR,
	.vop_rename = NULL_VOP_NOTDIR,
	.vop_readlink = NULL_VOP_NOTDIR,
	.vop_symlink = NULL_VOP_NOTDIR,
	.vop_namefile = NULL_VOP_NOTDIR,
	.vop_getdirentry = NULL_VOP_NOTDIR,
	.vop_reclaim = ffs_reclaim,
	.vop_ioctl = NULL_VOP_INVAL,
	.vop_gettype = ffs_gettype,
	.vop_tryseek = ffs_tryseek,
	.vop_truncate = ffs_truncfile,
	.vop_create = NULL_VOP_NOTDIR,
	.vop_unlink = NULL_VOP_NOTDIR,
	.vop_lookup = NULL_VOP_NOTDIR,
	.vop_lookup_parent = NULL_VOP_NOTDIR,
};
