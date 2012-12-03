#include <kio.h>
#include <console.h>
#include <glue_intr.h>
#include <stdio.h>
#include <spinlock.h>
#include <stdarg.h>
#include <unistd.h>
#include <mod.h>

/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt, int fd) {
    kcons_putc(c);
    (*cnt) ++;
}

static spinlock_s kprintf_lock = {0};

/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int
vkprintf(const char *fmt, va_list ap) {
    int cnt = 0;
	int flag;
	local_intr_save_hw(flag);
	spinlock_acquire(&kprintf_lock);
    vprintfmt((void*)cputch, NO_FD, &cnt, fmt, ap);
	spinlock_release(&kprintf_lock);
	local_intr_restore_hw(flag);
    return cnt;
}

/* *
 * cprintf - formats a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int
kprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vkprintf(fmt, ap);
    va_end(ap);
    return cnt;
}
EXPORT_SYMBOL(kprintf);