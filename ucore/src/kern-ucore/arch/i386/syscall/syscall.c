#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <clock.h>
#include <assert.h>
#include <sem.h>
#include <event.h>
#include <mbox.h>
#include <stat.h>
#include <dirent.h>
#include <sysfile.h>
#include <error.h>
#include <kio.h>

static uint32_t sys_exit(uint32_t arg[])
{
	int error_code = (int)arg[0];
	return do_exit(error_code);
}

static uint32_t sys_fork(uint32_t arg[])
{
	struct trapframe *tf = current->tf;
	uintptr_t stack = tf->tf_esp;
	return do_fork(0, stack, tf);
}

static uint32_t sys_wait(uint32_t arg[])
{
	int pid = (int)arg[0];
	int *store = (int *)arg[1];
	return do_wait(pid, store);
}

static uint32_t sys_exec(uint32_t arg[])
{
	const char *name = (const char *)arg[0];
	const char **argv = (const char **)arg[1];
	const char **envp = (const char **)arg[2];
	return do_execve(name, argv, envp);
}

static uint32_t sys_clone(uint32_t arg[])
{
	struct trapframe *tf = current->tf;
	uint32_t clone_flags = (uint32_t) arg[0];
	uintptr_t stack = (uintptr_t) arg[1];
	if (stack == 0) {
		stack = tf->tf_esp;
	}
	return do_fork(clone_flags, stack, tf);
}

static uint32_t sys_exit_thread(uint32_t arg[])
{
	int error_code = (int)arg[0];
	return do_exit_thread(error_code);
}

static uint32_t sys_yield(uint32_t arg[])
{
	return do_yield();
}

static uint32_t sys_sleep(uint32_t arg[])
{
	unsigned int time = (unsigned int)arg[0];
	return do_sleep(time);
}

static uint32_t sys_kill(uint32_t arg[])
{
	int pid = (int)arg[0];
	return do_kill(pid, -E_KILLED);
}

static uint32_t sys_gettime(uint32_t arg[])
{
	return (int)ticks;
}

static uint32_t sys_getpid(uint32_t arg[])
{
	return current->pid;
}

static uint32_t sys_brk(uint32_t arg[])
{
	uintptr_t *brk_store = (uintptr_t *) arg[0];
	return do_brk(brk_store);
}

static uint32_t sys_mmap(uint32_t arg[])
{
	uintptr_t *addr_store = (uintptr_t *) arg[0];
	size_t len = (size_t) arg[1];
	uint32_t mmap_flags = (uint32_t) arg[2];
	return do_mmap(addr_store, len, mmap_flags);
}

static uint32_t sys_munmap(uint32_t arg[])
{
	uintptr_t addr = (uintptr_t) arg[0];
	size_t len = (size_t) arg[1];
	return do_munmap(addr, len);
}

static uint32_t sys_shmem(uint32_t arg[])
{
	uintptr_t *addr_store = (uintptr_t *) arg[0];
	size_t len = (size_t) arg[1];
	uint32_t mmap_flags = (uint32_t) arg[2];
	return do_shmem(addr_store, len, mmap_flags);
}

static uint32_t sys_putc(uint32_t arg[])
{
	int c = (int)arg[0];
	cons_putc(c);
	return 0;
}

static uint32_t sys_pgdir(uint32_t arg[])
{
	print_pgdir(kprintf);
	return 0;
}

static uint32_t sys_sem_init(uint32_t arg[])
{
	int value = (int)arg[0];
	return ipc_sem_init(value);
}

static uint32_t sys_sem_post(uint32_t arg[])
{
	sem_t sem_id = (sem_t) arg[0];
	return ipc_sem_post(sem_id);
}

static uint32_t sys_sem_wait(uint32_t arg[])
{
	sem_t sem_id = (sem_t) arg[0];
	unsigned int timeout = (unsigned int)arg[1];
	return ipc_sem_wait(sem_id, timeout);
}

static uint32_t sys_sem_free(uint32_t arg[])
{
	sem_t sem_id = (sem_t) arg[0];
	return ipc_sem_free(sem_id);
}

static uint32_t sys_sem_get_value(uint32_t arg[])
{
	sem_t sem_id = (sem_t) arg[0];
	int *value_store = (int *)arg[1];
	return ipc_sem_get_value(sem_id, value_store);
}

static uint32_t sys_event_send(uint32_t arg[])
{
	int pid = (int)arg[0];
	int event = (int)arg[1];
	unsigned int timeout = (unsigned int)arg[2];
	return ipc_event_send(pid, event, timeout);
}

static uint32_t sys_event_recv(uint32_t arg[])
{
	int *pid_store = (int *)arg[0];
	int *event_store = (int *)arg[1];
	unsigned int timeout = (unsigned int)arg[2];
	return ipc_event_recv(pid_store, event_store, timeout);
}

static uint32_t sys_mbox_init(uint32_t arg[])
{
	unsigned int max_slots = (unsigned int)arg[0];
	return ipc_mbox_init(max_slots);
}

static uint32_t sys_mbox_send(uint32_t arg[])
{
	int id = (int)arg[0];
	struct mboxbuf *buf = (struct mboxbuf *)arg[1];
	unsigned int timeout = (unsigned int)arg[2];
	return ipc_mbox_send(id, buf, timeout);
}

static uint32_t sys_mbox_recv(uint32_t arg[])
{
	int id = (int)arg[0];
	struct mboxbuf *buf = (struct mboxbuf *)arg[1];
	unsigned int timeout = (unsigned int)arg[2];
	return ipc_mbox_recv(id, buf, timeout);
}

static uint32_t sys_mbox_free(uint32_t arg[])
{
	int id = (int)arg[0];
	return ipc_mbox_free(id);
}

static uint32_t sys_mbox_info(uint32_t arg[])
{
	int id = (int)arg[0];
	struct mboxinfo *info = (struct mboxinfo *)arg[1];
	return ipc_mbox_info(id, info);
}

static uint32_t sys_open(uint32_t arg[])
{
	const char *path = (const char *)arg[0];
	uint32_t open_flags = (uint32_t) arg[1];
	return sysfile_open(path, open_flags);
}

static uint32_t sys_close(uint32_t arg[])
{
	int fd = (int)arg[0];
	return sysfile_close(fd);
}

static uint32_t sys_read(uint32_t arg[])
{
	int fd = (int)arg[0];
	void *base = (void *)arg[1];
	size_t len = (size_t) arg[2];
	return sysfile_read(fd, base, len);
}

static uint32_t sys_write(uint32_t arg[])
{
	int fd = (int)arg[0];
	void *base = (void *)arg[1];
	size_t len = (size_t) arg[2];
	return sysfile_write(fd, base, len);
}

static uint32_t sys_seek(uint32_t arg[])
{
	int fd = (int)arg[0];
	off_t pos = (off_t) arg[1];
	int whence = (int)arg[2];
	return sysfile_seek(fd, pos, whence);
}

static uint32_t sys_fstat(uint32_t arg[])
{
	int fd = (int)arg[0];
	struct stat *stat = (struct stat *)arg[1];
	return sysfile_fstat(fd, stat);
}

static uint32_t sys_fsync(uint32_t arg[])
{
	int fd = (int)arg[0];
	return sysfile_fsync(fd);
}

static uint32_t sys_chdir(uint32_t arg[])
{
	const char *path = (const char *)arg[0];
	return sysfile_chdir(path);
}

static uint32_t sys_getcwd(uint32_t arg[])
{
	char *buf = (char *)arg[0];
	size_t len = (size_t) arg[1];
	return sysfile_getcwd(buf, len);
}

static uint32_t sys_mkdir(uint32_t arg[])
{
	const char *path = (const char *)arg[0];
	return sysfile_mkdir(path);
}

static uint32_t sys_link(uint32_t arg[])
{
	const char *path1 = (const char *)arg[0];
	const char *path2 = (const char *)arg[1];
	return sysfile_link(path1, path2);
}

static uint32_t sys_rename(uint32_t arg[])
{
	const char *path1 = (const char *)arg[0];
	const char *path2 = (const char *)arg[1];
	return sysfile_rename(path1, path2);
}

static uint32_t sys_unlink(uint32_t arg[])
{
	const char *name = (const char *)arg[0];
	return sysfile_unlink(name);
}

static uint32_t sys_getdirentry(uint32_t arg[])
{
	int fd = (int)arg[0];
	struct dirent *direntp = (struct dirent *)arg[1];
	return sysfile_getdirentry(fd, direntp, NULL);
}

static uint32_t sys_dup(uint32_t arg[])
{
	int fd1 = (int)arg[0];
	int fd2 = (int)arg[1];
	return sysfile_dup(fd1, fd2);
}

static uint32_t sys_pipe(uint32_t arg[])
{
	int *fd_store = (int *)arg[0];
	return sysfile_pipe(fd_store);
}

static uint32_t sys_mkfifo(uint32_t arg[])
{
	const char *name = (const char *)arg[0];
	uint32_t open_flags = (uint32_t) arg[1];
	return sysfile_mkfifo(name, open_flags);
}

static uint32_t sys_init_module(uint32_t arg[])
{
	void __user *umod = (void __user *)arg[0];
	unsigned long len = (unsigned long)arg[1];
	const char *urgs = (const char *)arg[2];
	return do_init_module(umod, len, urgs);
}

static uint32_t sys_cleanup_module(uint32_t arg[])
{
	const char __user *name = (const char __user *)arg[0];
	return do_cleanup_module(name);
}

static uint32_t sys_list_module(uint32_t arg[])
{
	print_modules();
	return 0;
}

static uint32_t sys_query_module(uint32_t arg[])
{
	const char* name=(const char*)arg[0];
	return find_module(name);
}

static uint32_t sys_mount(uint32_t arg[])
{
	const char *source = (const char *)arg[0];
	const char *target = (const char *)arg[1];
	const char *filesystemtype = (const char *)arg[2];
	const void *data = (const void *)arg[3];
	return do_mount(source, filesystemtype);
}

static uint32_t sys_umount(uint32_t arg[])
{
	const char *target = (const char *)arg[0];
	return do_umount(target);
}

static uint32_t(*syscalls[]) (uint32_t arg[]) = {
[SYS_exit] sys_exit,
	    [SYS_fork] sys_fork,
	    [SYS_wait] sys_wait,
	    [SYS_exec] sys_exec,
	    [SYS_clone] sys_clone,
	    [SYS_exit_thread] sys_exit_thread,
	    [SYS_yield] sys_yield,
	    [SYS_kill] sys_kill,
	    [SYS_sleep] sys_sleep,
	    [SYS_gettime] sys_gettime,
	    [SYS_getpid] sys_getpid,
	    [SYS_brk] sys_brk,
	    [SYS_mmap] sys_mmap,
	    [SYS_munmap] sys_munmap,
	    [SYS_shmem] sys_shmem,
	    [SYS_putc] sys_putc,
	    [SYS_pgdir] sys_pgdir,
	    [SYS_sem_init] sys_sem_init,
	    [SYS_sem_post] sys_sem_post,
	    [SYS_sem_wait] sys_sem_wait,
	    [SYS_sem_free] sys_sem_free,
	    [SYS_sem_get_value] sys_sem_get_value,
	    [SYS_event_send] sys_event_send,
	    [SYS_event_recv] sys_event_recv,
	    [SYS_mbox_init] sys_mbox_init,
	    [SYS_mbox_send] sys_mbox_send,
	    [SYS_mbox_recv] sys_mbox_recv,
	    [SYS_mbox_free] sys_mbox_free,
	    [SYS_mbox_info] sys_mbox_info,
	    [SYS_open] sys_open,
	    [SYS_close] sys_close,
	    [SYS_read] sys_read,
	    [SYS_write] sys_write,
	    [SYS_seek] sys_seek,
	    [SYS_fstat] sys_fstat,
	    [SYS_fsync] sys_fsync,
	    [SYS_chdir] sys_chdir,
	    [SYS_getcwd] sys_getcwd,
	    [SYS_mkdir] sys_mkdir,
	    [SYS_link] sys_link,
	    [SYS_rename] sys_rename,
	    [SYS_unlink] sys_unlink,
	    [SYS_getdirentry] sys_getdirentry,
	    [SYS_dup] sys_dup,
	    [SYS_pipe] sys_pipe,
	    [SYS_mkfifo] sys_mkfifo,
	    [SYS_init_module] sys_init_module,
	    [SYS_cleanup_module] sys_cleanup_module,
	    [SYS_list_module] sys_list_module,
	    [SYS_query_module] sys_query_module,
	    [SYS_mount] sys_mount,[SYS_umount] sys_umount,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void)
{
	struct trapframe *tf = current->tf;
	uint32_t arg[5];
	int num = tf->tf_regs.reg_eax;
	if (num >= 0 && num < NUM_SYSCALLS) {
		if (syscalls[num] != NULL) {
			arg[0] = tf->tf_regs.reg_edx;
			arg[1] = tf->tf_regs.reg_ecx;
			arg[2] = tf->tf_regs.reg_ebx;
			arg[3] = tf->tf_regs.reg_edi;
			arg[4] = tf->tf_regs.reg_esi;
			tf->tf_regs.reg_eax = syscalls[num] (arg);
			return;
		}
	}
	print_trapframe(tf);
	panic("undefined syscall %d, pid = %d, name = %s.\n",
	      num, current->pid, current->name);
}
