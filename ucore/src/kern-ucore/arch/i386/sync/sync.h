#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <arch.h>
#include <intr.h>
#include <mmu.h>
#include <assert.h>
#include <atomic.h>
#include <sched.h>

static inline bool __intr_save(void)
{
	if (read_eflags() & FL_IF) {
		intr_disable();
		return 1;
	}
	return 0;
}

static inline void __intr_restore(bool flag)
{
	if (flag) {
		intr_enable();
	}
}

static inline bool mod_init_save(void) 
{
	return __intr_save();
}
 
static inline void mod_init_restore(bool flag)
{
	return __intr_restore(flag);
}

#define local_intr_save(x)      do { x = __intr_save(); } while (0)
#define local_intr_restore(x)   __intr_restore(x);

void sync_init(void);

#endif /* !__KERN_SYNC_SYNC_H__ */
