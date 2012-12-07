#include <vmm.h>
#include <sync.h>
#include <slab.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <arch.h>
#include <swap.h>
#include <shmem.h>
#include <proc.h>
#include <sem.h>
#include <kio.h>

#include <file.h>
#include <proc.h>
#include <inode.h>


#define false	(0)

/* 
   vmm design include two parts: mm_struct (mm) & vma_struct (vma)
   mm is the memory manager for the set of continuous virtual memory  
   area which have the same PDT. vma is a continuous virtual memory area.
   There a linear link list for vma & a redblack link list for vma in mm.
   ---------------
   mm related functions:
   golbal functions
   struct mm_struct * mm_create(void)
   void mm_destroy(struct mm_struct *mm)
   int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
   --------------
   vma related functions:
   global functions
   struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
   void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
   struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
   inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
   inline struct vma_struct * find_vma_rb(rb_tree *tree, uintptr_t addr) 
   inline void insert_vma_rb(rb_tree *tree, struct vma_struct *vma, ....
   inline int vma_compare(rb_node *node1, rb_node *node2)
   ---------------
   check correctness functions
   void check_vmm(void);
   void check_vma_struct(void);
   void check_pgfault(void);
   */


#ifndef UCONFIG_HEAP_SLOB
#define __CHECK_MEMORY_LEAK() do{ \
  assert(nr_used_pages_store == nr_used_pages());   \
  assert(slab_allocated_store == slab_allocated()); \
  }while(0);

#else
#define __CHECK_MEMORY_LEAK() 
#endif

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

void
lock_mm(struct mm_struct *mm) {
  if (mm != NULL) {
    down(&(mm->mm_sem));
    struct proc_struct *current = pls_read(current);
    if (current != NULL) {
      mm->locked_by = current->pid;
    }
  }
}

void
unlock_mm(struct mm_struct *mm) {
  if (mm != NULL) {
    up(&(mm->mm_sem));
    mm->locked_by = 0;
  }
}

bool
try_lock_mm(struct mm_struct *mm) {
  if (mm != NULL) {
    if (!try_down(&(mm->mm_sem))) {
      return 0;
    }
    struct proc_struct *current = pls_read(current);
    if (current != NULL) {
      mm->locked_by = current->pid;
    }
  }
  return 1;
}

// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *
mm_create(void) {
  struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));
  if (mm != NULL) {
    list_init(&(mm->mmap_list));
    mm->mmap_tree = NULL;
    mm->mmap_cache = NULL;
    mm->pgdir = NULL;
    mm->map_count = 0;
    mm->swap_address = 0;
    set_mm_count(mm, 0);
    mm->locked_by = 0;
    mm->brk_start = mm->brk = 0;
    list_init(&(mm->proc_mm_link));
    sem_init(&(mm->mm_sem), 1);
    mm->lapic = -1;
  }
  return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
  struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
  if (vma != NULL) {
    vma->vm_start = vm_start;
    vma->vm_end = vm_end;
    vma->vm_flags = vm_flags;
    vma->shmem = NULL;
    vma->shmem_off = 0;
#ifdef UCONFIG_BIONIC_LIBC
	vma->mfile.file = NULL;
#endif //UCONFIG_BIONIC_LIBC
  }
  return vma;
}

// vma_destroy - free vma_struct
static void
vma_destroy(struct vma_struct *vma) {
  if (vma->vm_flags & VM_SHARE) {
    if (shmem_ref_dec(vma->shmem) == 0) {
      shmem_destroy(vma->shmem);
    }
  }
  kfree(vma);
}

// find_vma_rb - find a vma  (vma->vm_start <= addr <= vma_vm_end) in rb tree
static inline struct vma_struct *
find_vma_rb(rb_tree *tree, uintptr_t addr) {
  rb_node *node = rb_node_root(tree);
  struct vma_struct *vma = NULL, *tmp;
  while (node != NULL) {
    tmp = rbn2vma(node, rb_link);
    if (tmp->vm_end > addr) {
      vma = tmp;
      if (tmp->vm_start <= addr) {
        break;
      }
      node = rb_node_left(tree, node);
    }
    else {
      node = rb_node_right(tree, node);
    }
  }
  return vma;
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
  struct vma_struct *vma = NULL;
  if (mm != NULL) {
    vma = mm->mmap_cache;
    if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
      if (mm->mmap_tree != NULL) {
        vma = find_vma_rb(mm->mmap_tree, addr);
      }
      else {
        bool found = 0;
        list_entry_t *list = &(mm->mmap_list), *le = list;
        while ((le = list_next(le)) != list) {
          vma = le2vma(le, list_link);
          if (addr < vma->vm_end) {
            found = 1;
            break;
          }
        }
        if (!found) {
          vma = NULL;
        }
      }
    }
    if (vma != NULL) {
      mm->mmap_cache = vma;
    }
  }
  return vma;
}

struct vma_struct *
find_vma_intersection(struct mm_struct *mm, uintptr_t start, uintptr_t end) {
  struct vma_struct *vma = find_vma(mm, start);
  if (vma != NULL && end <= vma->vm_start) {
    vma = NULL;
  }
  return vma;
}

// vma_compare - compare vma1->vm_start < vma2->vm_start ?
static inline int
vma_compare(rb_node *node1, rb_node *node2) {
  struct vma_struct *vma1 = rbn2vma(node1, rb_link);
  struct vma_struct *vma2 = rbn2vma(node2, rb_link);
  uintptr_t start1 = vma1->vm_start, start2 = vma2->vm_start;
  return (start1 < start2) ? -1 : (start1 > start2) ? 1 : 0;
}

#ifdef UCONFIG_BIONIC_LIBC
void
vma_mapfile(struct vma_struct *vma, int fd, off_t off, struct fs_struct *fs_struct) {

	if(fs_struct == NULL) {
		fs_struct = pls_read(current)->fs_struct;
	}

	/*
	kprintf("mapfile:0x%08x fs_struct:0x%08x\n", vma->mfile.file, fs_struct);
	*/

	vma->mfile.offset = off;
	assert(vma != NULL);
	assert((vma->mfile.file = fd2file_onfs(fd, fs_struct)) != NULL);
	filemap_acquire(vma->mfile.file);
}

static void
vma_unmapfile(struct vma_struct *vma) {
	/*
	kprintf("un_mapfile:0x%08x\n", vma->mfile.file);
	*/

	if(vma->mfile.file != NULL) {
		filemap_release(vma->mfile.file);
	}
	vma->mfile.file = NULL;
}

static void
vma_copymapfile(struct vma_struct *to, struct vma_struct *from) {
	/*
	kprintf("copy_mapfile:0x%08x\n", from->mfile.file);
	*/

	to->mfile = from->mfile;
	if(to->mfile.file != NULL) {
		to->mfile.offset += to->vm_start - from->vm_start;
		filemap_acquire(to->mfile.file);
	}
}


#endif //UCONFIG_BIONIC_LIBC

// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
  assert(prev->vm_start < prev->vm_end);
  assert(prev->vm_end <= next->vm_start);
  assert(next->vm_start < next->vm_end);
}

// insert_vma_rb - insert vma in rb tree according vma->start_addr
static inline void
insert_vma_rb(rb_tree *tree, struct vma_struct *vma, struct vma_struct **vma_prevp) {
  rb_node *node = &(vma->rb_link), *prev;
  rb_insert(tree, node);
  if (vma_prevp != NULL) {
    prev = rb_node_prev(tree, node);
    *vma_prevp = (prev != NULL) ? rbn2vma(prev, rb_link) : NULL;
  }
}

// insert_vma_struct -insert vma in mm's rb tree link & list link
void
insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
  assert(vma->vm_start < vma->vm_end);
  list_entry_t *list = &(mm->mmap_list);
  list_entry_t *le_prev = list, *le_next;
  if (mm->mmap_tree != NULL) {
    struct vma_struct *mmap_prev;
    insert_vma_rb(mm->mmap_tree, vma, &mmap_prev);
    if (mmap_prev != NULL) {
      le_prev = &(mmap_prev->list_link);
    }
  }
  else {
    list_entry_t *le = list;
    while ((le = list_next(le)) != list) {
      struct vma_struct *mmap_prev = le2vma(le, list_link);
      if (mmap_prev->vm_start > vma->vm_start) {
        break;
      }
      le_prev = le;
    }
  }

  le_next = list_next(le_prev);

  /* check overlap */
  if (le_prev != list) {
    check_vma_overlap(le2vma(le_prev, list_link), vma);
  }
  if (le_next != list) {
    check_vma_overlap(vma, le2vma(le_next, list_link));
  }

  vma->vm_mm = mm;
  list_add_after(le_prev, &(vma->list_link));

  mm->map_count ++;
  if (mm->mmap_tree == NULL && mm->map_count >= RB_MIN_MAP_COUNT) {

    /* try to build red-black tree now, but may fail. */
    mm->mmap_tree = rb_tree_create(vma_compare);

    if (mm->mmap_tree != NULL) {
      list_entry_t *list = &(mm->mmap_list), *le = list;
      while ((le = list_next(le)) != list) {
        insert_vma_rb(mm->mmap_tree, le2vma(le, list_link), NULL);
      }
    }
  }
}

// remove_vma_struct - remove vma from mm's rb tree link & list link
static int
remove_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
  assert(mm == vma->vm_mm);
  if (mm->mmap_tree != NULL) {
    rb_delete(mm->mmap_tree, &(vma->rb_link));
  }
  list_del(&(vma->list_link));
  if (vma == mm->mmap_cache) {
    mm->mmap_cache = NULL;
  }
  mm->map_count --;
  return 0;
}

// mm_destroy - free mm and mm internal fields
void
mm_destroy(struct mm_struct *mm) {
  assert(mm_count(mm) == 0);
  if (mm->mmap_tree != NULL) {
    rb_tree_destroy(mm->mmap_tree);
  }
  list_entry_t *list = &(mm->mmap_list), *le;
  while ((le = list_next(list)) != list) {
    list_del(le);
    vma_destroy(le2vma(le, list_link));
  }
  kfree(mm);
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void
vmm_init(void) {
  check_vmm();
}

int
mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
    struct vma_struct **vma_store) {
  uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
  if (!USER_ACCESS(start, end)) {
    return -E_INVAL;
  }

  assert(mm != NULL);

  int ret = -E_INVAL;

  struct vma_struct *vma;
  if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
    goto out;
  }
  ret = -E_NO_MEM;
  vm_flags &= ~VM_SHARE;
  if ((vma = vma_create(start, end, vm_flags)) == NULL) {
    goto out;
  }
  insert_vma_struct(mm, vma);
  if (vma_store != NULL) {
    *vma_store = vma;
  }
  ret = 0;

out:
  return ret;
}

int
mm_map_shmem(struct mm_struct *mm, uintptr_t addr, uint32_t vm_flags,
    struct shmem_struct *shmem, struct vma_struct **vma_store) {
  if ((addr % PGSIZE) != 0 || shmem == NULL) {
    return -E_INVAL;
  }
  int ret;
  struct vma_struct *vma;
  shmem_ref_inc(shmem);
  if ((ret = mm_map(mm, addr, shmem->len, vm_flags, &vma)) != 0) {
    shmem_ref_dec(shmem);
    return ret;
  }
  vma->shmem = shmem;
  vma->shmem_off = 0;
  vma->vm_flags |= VM_SHARE;
  if (vma_store != NULL) {
    *vma_store = vma;
  }
  return 0;
}

static void
vma_resize(struct vma_struct *vma, uintptr_t start, uintptr_t end) {
  assert(start % PGSIZE == 0 && end % PGSIZE == 0);
  assert(vma->vm_start <= start && start < end && end <= vma->vm_end);
  if (vma->vm_flags & VM_SHARE) {
    vma->shmem_off += start - vma->vm_start;
  }

#ifdef UCONFIG_BIONIC_LIBC
  if(vma->mfile.file != NULL) {
	vma->mfile.offset += start - vma->vm_start;
  }
#endif //UCONFIG_BIONIC_LIBC

  vma->vm_start = start, vma->vm_end = end;
}

int
mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len) {
  uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
  if (!USER_ACCESS(start, end)) {
    return -E_INVAL;
  }

  assert(mm != NULL);

  struct vma_struct *vma;
  if ((vma = find_vma(mm, start)) == NULL || end <= vma->vm_start) {
    return 0;
  }

  if (vma->vm_start < start && end < vma->vm_end) {
    struct vma_struct *nvma;
    if ((nvma = vma_create(vma->vm_start, start, vma->vm_flags)) == NULL) {
      return -E_NO_MEM;
    }
#ifdef UCONFIG_BIONIC_LIBC
	vma_copymapfile(nvma, vma);
#endif //UCONFIG_BIONIC_LIBC
    vma_resize(vma, end, vma->vm_end);
    insert_vma_struct(mm, nvma);
    unmap_range(mm->pgdir, start, end);
    return 0;
  }

  list_entry_t free_list, *le;
  list_init(&free_list);
  while (vma->vm_start < end) {
    le = list_next(&(vma->list_link));
    remove_vma_struct(mm, vma);
    list_add(&free_list, &(vma->list_link));
    if (le == &(mm->mmap_list)) {
      break;
    }
    vma = le2vma(le, list_link);
  }

  le = list_next(&free_list);
  while (le != &free_list) {
    vma = le2vma(le, list_link);
    le = list_next(le);
    uintptr_t un_start, un_end;
    if (vma->vm_start < start) {
      un_start = start, un_end = vma->vm_end;
      vma_resize(vma, vma->vm_start, un_start);
      insert_vma_struct(mm, vma);
    }
    else {
      un_start = vma->vm_start, un_end = vma->vm_end;
      if (end < un_end) {
        un_end = end;
        vma_resize(vma, un_end, vma->vm_end);
        insert_vma_struct(mm, vma);
      }
      else {
#ifdef UCONFIG_BIONIC_LIBC
		vma_unmapfile(vma);
#endif //UCONFIG_BIONIC_LIBC
        vma_destroy(vma);
      }
    }
    unmap_range(mm->pgdir, un_start, un_end);
  }
  return 0;
}

#ifdef UCONFIG_BIONIC_LIBC
int mm_unmap_keep_pages(struct mm_struct *mm, uintptr_t addr, size_t len) {
	uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
	if(!USER_ACCESS(start, end)) {
		return -E_INVAL;
	}

	assert(mm != NULL);

	struct vma_struct *vma;
	if((vma = find_vma(mm, start)) == NULL || end <= vma->vm_start) {
		return 0;
	}

	if(vma->vm_start < start && end < vma->vm_end) {
		struct vma_struct *nvma;
		if((nvma = vma_create(vma->vm_start, start, vma->vm_flags)) == NULL) {
			return -E_NO_MEM;
		}

		vma_copymapfile(nvma, vma);
		vma_resize(vma, end, vma->vm_end);
		insert_vma_struct(mm, nvma);

		return 0;
	}

	list_entry_t free_list, *le;
	list_init(&free_list);
	while(vma->vm_start < end) {
		le = list_next(&(vma->list_link));
		remove_vma_struct(mm, vma);
		list_add(&free_list, &(vma->list_link));
		if(le == &(mm->mmap_list)) {
			break;
		}
		vma = le2vma(le, list_link);
	}

	le = list_next(&free_list);
	while(le != &free_list) {
		vma = le2vma(le, list_link);
		le = list_next(le);
		uintptr_t un_start, un_end;
		if(vma->vm_start < start) {
			un_start = start, un_end = vma->vm_end;
			vma_resize(vma, vma->vm_start, un_start);
			insert_vma_struct(mm, vma);
		} else {
			un_start = vma->vm_start, un_end = vma->vm_end;
			if(end < un_end) {
				un_end = end;
				vma_resize(vma, un_end, vma->vm_end);
				insert_vma_struct(mm, vma);
			} else {
				vma_unmapfile(vma);
				vma_destroy(vma);
			}
		}
		//unmap_range(mm->pgdir, un_start, un_end);
	}
	return 0;
}
#endif //UCONFIG_BIONIC_LIBC

int
dup_mmap(struct mm_struct *to, struct mm_struct *from) {
  assert(to != NULL && from != NULL);
  list_entry_t *list = &(from->mmap_list), *le = list;
  while ((le = list_prev(le)) != list) {
    struct vma_struct *vma, *nvma;
    vma = le2vma(le, list_link);
    nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
    if (nvma == NULL) {
      return -E_NO_MEM;
    }
    else {
      if (vma->vm_flags & VM_SHARE) {
        nvma->shmem = vma->shmem;
        nvma->shmem_off = vma->shmem_off;
        shmem_ref_inc(vma->shmem);
      }
#ifdef UCONFIG_BIONIC_LIBC
	  nvma->mfile = vma->mfile;
#endif //UCONFIG_BIONIC_LIBC
    }
    insert_vma_struct(to, nvma);
    bool share = (vma->vm_flags & VM_SHARE);
    if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share) != 0) {
      return -E_NO_MEM;
    }
  }
  return 0;
}

void
exit_mmap(struct mm_struct *mm) {
  assert(mm != NULL && mm_count(mm) == 0);
  pgd_t *pgdir = mm->pgdir;
  list_entry_t *list = &(mm->mmap_list), *le = list;
  while ((le = list_next(le)) != list) {
    struct vma_struct *vma = le2vma(le, list_link);
    unmap_range(pgdir, vma->vm_start, vma->vm_end);

#ifdef UCONFIG_BIONIC_LIBC
	vma_unmapfile(vma);
#endif //UCONFIG_BIONIC_LIBC
  }
  while ((le = list_next(le)) != list) {
    struct vma_struct *vma = le2vma(le, list_link);
    exit_range(pgdir, vma->vm_start, vma->vm_end);
  }
}

#ifdef UCONFIG_BIONIC_LIBC
int
remapfile(struct mm_struct *mm, struct proc_struct *proc) {
		//kprintf("remap!!\n");
	assert(mm != NULL);
	list_entry_t *list = &(mm->mmap_list), *le = list;
	while((le = list_prev(le)) != list) {
		struct vma_struct *vma = le2vma(le, list_link);
		if(vma->mfile.file != NULL) {
				/*
				kprintf("remapfile:0x%08x fd:%d offset:0x%08x\n", vma->mfile.file, vma->mfile.file->fd,
								vma->mfile.offset);
				*/
			vma_mapfile(vma, vma->mfile.file->fd, vma->mfile.offset, proc->fs_struct);
		}
	}
	return 0;
}
#endif //UCONFIG_BIONIC_LIBC

uintptr_t
get_unmapped_area(struct mm_struct *mm, size_t len) {
  if (len == 0 || len > USERTOP) {
    return 0;
  }
  uintptr_t start = USERTOP - len;
  list_entry_t *list = &(mm->mmap_list), *le = list;
  while ((le = list_prev(le)) != list) {
    struct vma_struct *vma = le2vma(le, list_link);
    if (start >= vma->vm_end) {
      return start;
    }
    if (start + len > vma->vm_start) {
      if (len >= vma->vm_start) {
        return 0;
      }
      start = vma->vm_start - len;
    }
  }
  return (start >= USERBASE) ? start : 0;
}

int
mm_brk(struct mm_struct *mm, uintptr_t addr, size_t len) {
  uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
  if (!USER_ACCESS(start, end)) {
    return -E_INVAL;
  }

  int ret;
  if ((ret = mm_unmap(mm, start, end - start)) != 0) {
    return ret;
  }
  uint32_t vm_flags = VM_READ | VM_WRITE;
  struct vma_struct *vma = find_vma(mm, start - 1);
  if (vma != NULL && vma->vm_end == start && vma->vm_flags == vm_flags) {
    vma->vm_end = end;
    return 0;
  }
  if ((vma = vma_create(start, end, vm_flags)) == NULL) {
    return -E_NO_MEM;
  }
  insert_vma_struct(mm, vma);
  return 0;
}

bool
user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write) {
  if (mm != NULL) {
    if (!USER_ACCESS(addr, addr + len)) {
      return 0;
    }
    struct vma_struct *vma;
    uintptr_t start = addr, end = addr + len;
    while (start < end) {
      if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start) {
        return 0;
      }
      if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ))) {
        return 0;
      }
      if (write && (vma->vm_flags & VM_STACK)) {
        if (start < vma->vm_start + PGSIZE) {
          return 0;
        }
      }
      start = vma->vm_end;
    }
    return 1;
  }
  return KERN_ACCESS(addr, addr + len);
}

// check_vmm - check correctness of vmm
static void
check_vmm(void) {
  size_t nr_used_pages_store = nr_used_pages();
  size_t slab_allocated_store = slab_allocated();

  check_vma_struct();
  check_pgfault();

  __CHECK_MEMORY_LEAK();

  kprintf("check_vmm() succeeded.\n");
}

static void
check_vma_struct(void) {
  size_t nr_used_pages_store = nr_used_pages();
  size_t slab_allocated_store = slab_allocated();

  struct mm_struct *mm = mm_create();
  assert(mm != NULL);

  int step1 = RB_MIN_MAP_COUNT * 2, step2 = step1 * 10;

  int i;
  for (i = step1; i >= 0; i --) {
    struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
    assert(vma != NULL);
    insert_vma_struct(mm, vma);
  }

  for (i = step1 + 1; i <= step2; i ++) {
    struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
    assert(vma != NULL);
    insert_vma_struct(mm, vma);
  }

  list_entry_t *le = list_next(&(mm->mmap_list));

  for (i = 0; i <= step2; i ++) {
    assert(le != &(mm->mmap_list));
    struct vma_struct *mmap = le2vma(le, list_link);
    assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
    le = list_next(le);
  }

  for (i = 0; i < 5 * step2 + 2; i ++) {
    struct vma_struct *vma = find_vma(mm, i);
    assert(vma != NULL);
    int j = i / 5;
    if (i >= 5 * j + 2) {
      j ++;
    }
    assert(vma->vm_start == j * 5 && vma->vm_end == j * 5 + 2);
  }

  mm_destroy(mm);
  
  __CHECK_MEMORY_LEAK();

  kprintf("check_vma_struct() succeeded!\n");
}

struct mm_struct *check_mm_struct;

// check_pgfault - check correctness of pgfault handler
static void
check_pgfault(void) {
#ifdef UCONFIG_CHECK_PGFAULT
  kprintf("starting check_pgfault()\n");
  size_t nr_used_pages_store = nr_used_pages();
  size_t slab_allocated_store = slab_allocated();

  check_mm_struct = mm_create();
  assert(check_mm_struct != NULL);

  struct mm_struct *mm = check_mm_struct;
  pgd_t *pgdir = mm->pgdir = init_pgdir_get();
  assert(pgdir[PGX(TEST_PAGE)] == 0);

  struct vma_struct *vma = vma_create(TEST_PAGE, TEST_PAGE + PTSIZE, VM_WRITE);
  assert(vma != NULL);

  insert_vma_struct(mm, vma);
  uintptr_t addr = TEST_PAGE + 0x100;
  assert(find_vma(mm, addr) == vma);

  int i, sum = 0;
  for (i = 0; i < 100; i ++) {
    *(char *)(addr + i) = i;
    sum += i;
  }
  for (i = 0; i < 100; i ++) {
    sum -= *(char *)(addr + i);
  }
  assert(sum == 0);

  page_remove(pgdir, ROUNDDOWN(addr, PGSIZE));
#if PMXSHIFT != PUXSHIFT
  free_page(pa2page(PMD_ADDR(*get_pmd(pgdir, addr, 0))));
#endif
#if PUXSHIFT != PGXSHIFT
  free_page(pa2page(PUD_ADDR(*get_pud(pgdir, addr, 0))));
#endif
  free_page(pa2page(PGD_ADDR(*get_pgd(pgdir, addr, 0))));
  pgdir[PGX(TEST_PAGE)] = 0;

  mm->pgdir = NULL;
  mm_destroy(mm);
  check_mm_struct = NULL;

  assert(nr_used_pages_store == nr_used_pages());
  assert(slab_allocated_store == slab_allocated());

  kprintf("check_pgfault() succeeded!\n");
#endif
}

#ifdef UCONFIG_BIONIC_LIBC
static inline struct mapped_addr *
find_maddr(struct file *file, off_t offset, struct Page *page) {
	list_entry_t *list = &(file->node->mapped_addr_list);
	list_entry_t *le = list;
	while((le = list_next(le)) != list) {
		struct mapped_addr *maddr = le2maddr(le);
		if(maddr->offset = offset && (page == NULL || maddr->page == page)) {
			return maddr;
		}
	}
	return NULL;
}
#endif //UCONFIG_BIONIC_LIBC

int 
do_madvise(void *addr, size_t len, int advice) {
	return 0;
}


int
do_pgfault(struct mm_struct *mm, machine_word_t error_code, uintptr_t addr) {
  struct proc_struct *current = pls_read(current);
  if (mm == NULL) {
    assert(current != NULL);
    /* Chen Yuheng 
     * give handler a chance to deal with it 
     */
    kprintf("page fault in kernel thread: pid = %d, name = %s, %d %08x.\n",
        current->pid, current->name ,error_code, addr);
    return -E_KILLED;
  }

  bool need_unlock = 1;
  if (!try_lock_mm(mm)) {
    if (current != NULL && mm->locked_by == current->pid) {
      need_unlock = 0;
    }
    else {
      lock_mm(mm);
    }
  }

  int ret = -E_INVAL;
  struct vma_struct *vma = find_vma(mm, addr);
  if (vma == NULL || vma->vm_start > addr) {
    goto failed;
  }
  if (vma->vm_flags & VM_STACK) {
    if (addr < vma->vm_start + PGSIZE) {
      goto failed;
    }
  }

  //kprintf("@ %x %08x\n", vma->vm_flags, vma->vm_start);
  //assert((vma->vm_flags & VM_IO)==0);
  if(vma->vm_flags & VM_IO){
    ret = -E_INVAL;
    goto failed;
  }
  switch (error_code & 3) {
    default:
      /* default is 3: write, present */
    case 2: /* write, not present */
      if (!(vma->vm_flags & VM_WRITE)) {
        goto failed;
      }
      break;
    case 1: /* read, present */
      goto failed;
    case 0: /* read, not present */
      if (!(vma->vm_flags & (VM_READ | VM_EXEC))) {
        goto failed;
      }
  }

  pte_perm_t perm, nperm;
#ifdef ARCH_ARM
#warning ARM9 software emulated PTE_xxx
  perm = PTE_P|PTE_U;
  if (vma->vm_flags & VM_WRITE) {
    perm |= PTE_W;
  }
#else
  ptep_unmap (&perm);
  ptep_set_u_read(&perm);
  if (vma->vm_flags & VM_WRITE) {
    ptep_set_u_write(&perm);
  }
#endif
  addr = ROUNDDOWN(addr, PGSIZE);

  ret = -E_NO_MEM;

  pte_t *ptep;
  if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL) {
    goto failed;
  }
  if (ptep_invalid(ptep)) {
#ifdef UCONFIG_BIONIC_LIBC
	if(vma->mfile.file != NULL) {
		struct file *file = vma->mfile.file;
		off_t old_pos = file->pos, new_pos = vma->mfile.offset + addr - vma->vm_start;
#ifdef SHARE_MAPPED_FILE
		struct mapped_addr *maddr = find_maddr(file, new_pos, NULL);
		if(maddr == NULL) {
#endif // SHARE_MAPPED_FILE
			struct Page *page;
			if((page = alloc_page()) == NULL) {
				assert(false);
				goto failed;
			}
			nperm = perm;
#ifdef ARCH_ARM
#warning ARM9 software emulated PTE_xxx
			nperm &= ~PTE_W;
#else
			ptep_unset_s_write(&nperm);
#endif
			page_insert_pte(mm->pgdir, page, ptep, addr, nperm);

			if((ret = filestruct_setpos(file, new_pos)) != 0) {
				assert(false);
				goto failed;
			}
			filestruct_read(file, page2kva(page), PGSIZE);
			if((ret = filestruct_setpos(file, old_pos)) != 0) {
				assert(false);
				goto failed;
			}
#ifdef SHARE_MAPPED_FILE
			if((maddr = (struct mapped_addr*)kmalloc(sizeof(struct mapped_addr))) != NULL) {
				maddr->page = page;
				maddr->offset = new_pos;
				page->maddr = maddr;
				list_add(&(file->node->mapped_addr_list), &(maddr->list));
			} else {
				assert(false);
			}
		} else {
			nperm = perm;
#ifdef ARCH_ARM
#warning ARM9 software emulated PTE_xxx
			nperm &= ~PTE_W;
#else
			ptep_unset_s_write(&nperm);
#endif
			page_insert_pte(mm->pgdir, maddr->page, ptep, addr, nperm);
		}
#endif //SHARE_MAPPED_FILE

	} else
#endif //UCONFIG_BIONIC_LIBC
			if (!(vma->vm_flags & VM_SHARE)) {
      if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
        goto failed;
      }
#ifdef UCONFIG_BIONIC_LIBC
	  if(vma->vm_flags & VM_ANONYMOUS) {
		memset((void*)addr, 0, PGSIZE);
	  }
#endif //UCONFIG_BIONIC_LIBC
    }
    else { //shared mem
      lock_shmem(vma->shmem);
      uintptr_t shmem_addr = addr - vma->vm_start + vma->shmem_off;
      pte_t *sh_ptep = shmem_get_entry(vma->shmem, shmem_addr, 1);
      if (sh_ptep == NULL || ptep_invalid(sh_ptep)) {
        unlock_shmem(vma->shmem);
        goto failed;
      }
      unlock_shmem(vma->shmem);
      if (ptep_present(sh_ptep)) {
        page_insert(mm->pgdir, pa2page(*sh_ptep), addr, perm);
      }
      else {
#ifdef UCONFIG_SWAP
        swap_duplicate(*ptep);
        ptep_copy(ptep, sh_ptep);
#else
        panic("NO SWAP\n");
#endif
      }
    }
  }else { //a present page, handle copy-on-write (cow) 
    struct Page *page, *newpage = NULL;
    bool cow = ((vma->vm_flags & (VM_SHARE | VM_WRITE)) == VM_WRITE), may_copy = 1;

#if 1
    if (!(!ptep_present(ptep) || ((error_code & 2) && !ptep_u_write(ptep) && cow)))
    {
      //assert(PADDR(mm->pgdir) == rcr3());
      kprintf("%p %p %d %d %x\n", *ptep, addr, error_code, cow, vma->vm_flags);
      assert(0);
    }
#endif

    if (cow) {
      newpage = alloc_page();
    }
    if (ptep_present(ptep)) {
      page = pte2page(*ptep);
    }
    else {
#ifdef UCONFIG_SWAP
      if ((ret = swap_in_page(*ptep, &page)) != 0) {
        if (newpage != NULL) {
          free_page(newpage);
        }
        goto failed;
      }
#else
      assert(0);
#endif
      if (!(error_code & 2) && cow) {
#ifdef ARCH_ARM
#warning ARM9 software emulated PTE_xxx
        perm &= ~PTE_W;
#else
        ptep_unset_s_write(&perm);
#endif
        may_copy = 0;
      }
    }

    if (cow && may_copy) {
#ifdef UCONFIG_SWAP
      if (page_ref(page) + swap_page_count(page) > 1) {
#else
      if (page_ref(page) > 1){
#endif
        if (newpage == NULL) {
          goto failed;
        }
        memcpy(page2kva(newpage), page2kva(page), PGSIZE);
        //kprintf("COW!\n");
        page = newpage, newpage = NULL;
      }
    }
#ifdef UCONFIG_BIONIC_LIBC
	  else if(vma->mfile.file != NULL) {
#ifdef UCONFIG_SWAP
		assert(page_reg(page) + swap_page_count(page) == 1);
#else
		assert(page_ref(page) == 1);
#endif

#ifdef SHARE_MAPPED_FILE
		off_t offset = vma->mfile.offset + addr - vma->vm_start;
		struct mapped_addr *maddr = find_maddr(vma->mfile.file, offset, page);
		if(maddr != NULL) {
			list_del(&(maddr->list));
			kfree(maddr);
			page->maddr = NULL;
			assert(find_maddr(vma->mfile.file, offset, page) == NULL);
		} else {
		}
#endif //SHARE_MAPPED_FILE
	}
#endif //UCONFIG_BIONIC_LIBC
	else {
	}
    page_insert(mm->pgdir, page, addr, perm);
    if (newpage != NULL) {
      free_page(newpage);
    }
  }
  ret = 0;

failed:
  if (need_unlock) {
    unlock_mm(mm);
  }
  return ret;
}

