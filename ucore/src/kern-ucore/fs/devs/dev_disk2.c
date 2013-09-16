#include <types.h>
#include <mmu.h>
#include <slab.h>
#include <sem.h>
#include <ide.h>
#include <inode.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

#define DISK2_BLKSIZE                   PGSIZE
#define DISK2_BUFSIZE                   (4 * DISK2_BLKSIZE)
#define DISK2_BLK_NSECT                 (DISK2_BLKSIZE / SECTSIZE)

static char *disk2_buffer;
static semaphore_t disk2_sem;

static void lock_disk2(void)
{
	down(&(disk2_sem));
}

static void unlock_disk2(void)
{
	up(&(disk2_sem));
}

static int disk2_open(struct device *dev, uint32_t open_flags)
{
	return 0;
}

static int disk2_close(struct device *dev)
{
	return 0;
}

static void disk2_read_blks_nolock(uint32_t blkno, uint32_t nblks)
{
	kprintf("in disk2_read_blks_nolock()\n");
	int ret;
	uint32_t sectno = blkno * DISK2_BLK_NSECT, nsecs =
	    nblks * DISK2_BLK_NSECT;
	if ((ret =
	     ide_read_secs(DISK2_DEV_NO, sectno, disk2_buffer, nsecs)) != 0) {
		panic
		    ("disk2: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
		     blkno, sectno, nblks, nsecs, ret);
	}
}

static void disk2_write_blks_nolock(uint32_t blkno, uint32_t nblks)
{
	int ret;
	uint32_t sectno = blkno * DISK2_BLK_NSECT, nsecs =
	    nblks * DISK2_BLK_NSECT;
	if ((ret =
	     ide_write_secs(DISK2_DEV_NO, sectno, disk2_buffer, nsecs)) != 0) {
		panic
		    ("disk2: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
		     blkno, sectno, nblks, nsecs, ret);
	}
}

static int disk2_io(struct device *dev, struct iobuf *iob, bool write)
{
	off_t offset = iob->io_offset;
	size_t resid = iob->io_resid;
	uint32_t blkno = offset / DISK2_BLKSIZE;
	uint32_t nblks = resid / DISK2_BLKSIZE;

	/* don't allow I/O that isn't block-aligned */
	if ((offset % DISK2_BLKSIZE) != 0 || (resid % DISK2_BLKSIZE) != 0) {
		return -E_INVAL;
	}

	/* don't allow I/O past the end of disk2 */
	if (blkno + nblks > dev->d_blocks) {
		return -E_INVAL;
	}

	/* read/write nothing ? */
	if (nblks == 0) {
		return 0;
	}

	lock_disk2();
	while (resid != 0) {
		size_t copied, alen = DISK2_BUFSIZE;
		if (write) {
			iobuf_move(iob, disk2_buffer, alen, 0, &copied);
			assert(copied != 0 && copied <= resid
			       && copied % DISK2_BLKSIZE == 0);
			nblks = copied / DISK2_BLKSIZE;
			disk2_write_blks_nolock(blkno, nblks);
		} else {
			if (alen > resid) {
				alen = resid;
			}
			nblks = alen / DISK2_BLKSIZE;
			disk2_read_blks_nolock(blkno, nblks);
			iobuf_move(iob, disk2_buffer, alen, 1, &copied);
			assert(copied == alen && copied % DISK2_BLKSIZE == 0);
		}
		resid -= copied, blkno += nblks;
	}
	unlock_disk2();
	return 0;
}

static int disk2_ioctl(struct device *dev, int op, void *data)
{
	return -E_UNIMP;
}

static void disk2_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	static_assert(DISK2_BLKSIZE % SECTSIZE == 0);
	if (!ide_device_valid(DISK2_DEV_NO)) {
		panic("disk2 device isn't available.\n");
	}
	dev->d_blocks = ide_device_size(DISK2_DEV_NO) / DISK2_BLK_NSECT;
	dev->d_blocksize = DISK2_BLKSIZE;
	dev->d_open = disk2_open;
	dev->d_close = disk2_close;
	dev->d_io = disk2_io;
	dev->d_ioctl = disk2_ioctl;
	sem_init(&(disk2_sem), 1);

	static_assert(DISK2_BUFSIZE % DISK2_BLKSIZE == 0);
	if ((disk2_buffer = kmalloc(DISK2_BUFSIZE)) == NULL) {
		panic("disk2 alloc buffer failed.\n");
	}
}

void dev_init_disk2(void)
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("disk2: dev_create_node.\n");
	}
	disk2_device_init(vop_info(node, device));

	int ret;
	if ((ret = vfs_add_dev("disk2", node, 1)) != 0) {
		panic("disk2: vfs_add_dev: %e.\n", ret);
	}
}
