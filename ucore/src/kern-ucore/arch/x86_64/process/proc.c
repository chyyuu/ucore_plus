#include <proc.h>
#include <pmm.h>
#include <slab.h>
#include <string.h>
#include <types.h>
#include <stdlib.h>
#include <mp.h>

void forkret(void);
void forkrets(struct trapframe *tf);

// alloc_proc - create a proc struct and init fields
struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = PADDR(init_pgdir_get());
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
        proc->wait_state = 0;
        proc->cptr = proc->optr = proc->yptr = NULL;
        list_init(&(proc->thread_group));
        proc->rq = NULL;
        list_init(&(proc->run_link));
        proc->time_slice = 0;
        proc->sem_queue = NULL;
        event_box_init(&(proc->event_box));
        proc->fs_struct = NULL;
    }
    return proc;
}

int
copy_thread(uint32_t clone_flags, struct proc_struct *proc, uintptr_t rsp, struct trapframe *tf) {
    uintptr_t kstacktop = proc->kstack + KSTACKSIZE;
    proc->tf = (struct trapframe *)kstacktop - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_rax = 0;
    proc->tf->tf_rsp = (rsp != 0) ? rsp : kstacktop;
    proc->tf->tf_rflags |= FL_IF;

    proc->context.rip = (uintptr_t)forkret;
    proc->context.rsp = (uintptr_t)(proc->tf);

	return 0;
}


// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_rdi = (uint64_t)fn;
    tf.tf_regs.reg_rsi = (uint64_t)arg;
    tf.tf_rip = (uint64_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
void
forkret(void) {
	if (!trap_in_kernel(pls_read(current)->tf))
	{
		kern_leave();
	}
    forkrets(pls_read(current)->tf);
}

int
kernel_execve(const char *name, const char **argv, const char **kenvp) {
    int ret;
    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL), "0" (SYS_exec), "D" (name), "S" (argv), "d" (kenvp)
        : "memory");
    return ret;
}

#define current (pls_read(current))

int
init_new_context (struct proc_struct *proc, struct elfhdr *elf,
		  int argc, char** kargv, int envc, char **kenvp) {
    uintptr_t stacktop = USTACKTOP - argc * PGSIZE;
    char **uargv = (char **)(stacktop - argc * sizeof(char *));
    int i;
    for (i = 0; i < argc; i ++) {
        uargv[i] = strcpy((char *)(stacktop + i * PGSIZE), kargv[i]);
    }
    stacktop = (uintptr_t)uargv;

    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->tf_cs = USER_CS;
    tf->tf_ds = USER_DS;
    tf->tf_es = USER_DS;
    tf->tf_ss = USER_DS;
    tf->tf_rsp = stacktop;
    tf->tf_rip = elf->e_entry;
    tf->tf_rflags = FL_IF;
    tf->tf_regs.reg_rdi = argc;
    tf->tf_regs.reg_rsi = (uintptr_t)uargv;

	return 0;
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void
cpu_idle(void) {
    while (1) {
		assert((read_rflags() & FL_IF) != 0);
		asm volatile ("hlt");
    }
}

int
do_execve_arch_hook (int argc, char** kargv) {
	return 0;
}

int
ucore_kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
       kernel_thread(fn, arg, clone_flags);
}

void
de_thread_arch_hook (struct proc_struct *proc) {}
