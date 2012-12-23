#include <types.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#include <mboxbuf.h>
#include <stat.h>
#include <dirent.h>
#include <signal.h>

#ifndef ARCH_ARM

extern uintptr_t syscall (int num, ...);

int
sys_exit(int error_code) {
    return syscall(SYS_exit, error_code);
}

int
sys_fork(void) {
    return syscall(SYS_fork);
}

int
sys_wait(int pid, int *store) {
    return syscall(SYS_wait, pid, store);
}

int
sys_exec(const char *filename, const char **argv, const char **envp) {
    return syscall(SYS_exec, filename, argv, envp);
}

int
sys_yield(void) {
    return syscall(SYS_yield);
}

int
sys_sleep(unsigned int time) {
    return syscall(SYS_sleep, time);
}

int
sys_kill(int pid) {
    return syscall(SYS_kill, pid);
}

size_t
sys_gettime(void) {
    return (size_t)syscall(SYS_gettime);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid);
}

int
sys_brk(uintptr_t *brk_store) {
    return syscall(SYS_brk, brk_store);
}

int
sys_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return syscall(SYS_mmap, addr_store, len, mmap_flags);
}

int
sys_munmap(uintptr_t addr, size_t len) {
    return syscall(SYS_munmap, addr, len);
}

int
sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return syscall(SYS_shmem, addr_store, len, mmap_flags);
}

int
sys_putc(int c) {
    return syscall(SYS_putc, c);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir);
}

sem_t
sys_sem_init(int value) {
    return syscall(SYS_sem_init, value);
}

int
sys_sem_post(sem_t sem_id) {
    return syscall(SYS_sem_post, sem_id);
}

int
sys_sem_wait(sem_t sem_id, unsigned int timeout) {
    return syscall(SYS_sem_wait, sem_id, timeout);
}

int
sys_sem_free(sem_t sem_id) {
    return syscall(SYS_sem_free, sem_id);
}

int
sys_sem_get_value(sem_t sem_id, int *value_store) {
    return syscall(SYS_sem_get_value, sem_id, value_store);
}

int
sys_send_event(int pid, int event, unsigned int timeout) {
    return syscall(SYS_event_send, pid, event, timeout);
}

int
sys_recv_event(int *pid_store, int *event_store, unsigned int timeout) {
    return syscall(SYS_event_recv, pid_store, event_store, timeout);
}

int
sys_mbox_init(unsigned int max_slots) {
    return syscall(SYS_mbox_init, max_slots);
}

int
sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout) {
    return syscall(SYS_mbox_send, id, buf, timeout);
}

int
sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout) {
    return syscall(SYS_mbox_recv, id, buf, timeout);
}

int
sys_mbox_free(int id) {
    return syscall(SYS_mbox_free, id);
}

int
sys_mbox_info(int id, struct mboxinfo *info) {
    return syscall(SYS_mbox_info, id, info);
}

int
sys_open(const char *path, uint32_t open_flags) {
    return syscall(SYS_open, path, open_flags);
}

int
sys_close(int fd) {
    return syscall(SYS_close, fd);
}

int
sys_read(int fd, void *base, size_t len) {
    return syscall(SYS_read, fd, base, len);
}

int
sys_write(int fd, void *base, size_t len) {
    return syscall(SYS_write, fd, base, len);
}

int
sys_seek(int fd, off_t pos, int whence) {
    return syscall(SYS_seek, fd, pos, whence);
}

int
sys_fstat(int fd, struct stat *stat) {
    return syscall(SYS_fstat, fd, stat);
}

int
sys_fsync(int fd) {
    return syscall(SYS_fsync, fd);
}

int
sys_chdir(const char *path) {
    return syscall(SYS_chdir, path);
}

int
sys_getcwd(char *buffer, size_t len) {
    return syscall(SYS_getcwd, buffer, len);
}

int
sys_mkdir(const char *path) {
    return syscall(SYS_mkdir, path);
}

int
sys_link(const char *path1, const char *path2) {
    return syscall(SYS_link, path1, path2);
}

int
sys_rename(const char *path1, const char *path2) {
    return syscall(SYS_rename, path1, path2);
}

int
sys_unlink(const char *path) {
    return syscall(SYS_unlink, path);
}

int
sys_getdirentry(int fd, struct dirent *dirent) {
    return syscall(SYS_getdirentry, fd, dirent);
}

int
sys_dup(int fd1, int fd2) {
    return syscall(SYS_dup, fd1, fd2);
}

int
sys_pipe(int *fd_store) {
    return syscall(SYS_pipe, fd_store);
}

int
sys_mkfifo(const char *name, uint32_t open_flags) {
    return syscall(SYS_mkfifo, name, open_flags);
}

int
sys_ioctl(int d, int request, unsigned long data) {
    return syscall(SYS_ioctl, d, request, data);
}

int 
sys_init_module(void __user *umod, unsigned long len, const char __user *uargs) {
    return syscall(SYS_init_module, umod, len, uargs);
}

int 
sys_cleanup_module(const char __user *name) {
    return syscall(SYS_cleanup_module, name);
}

int 
sys_list_module() {
    return syscall(SYS_list_module);
}

int 
sys_mount(const char *source, const char *target, const char* filesystemtype, const void *data) {
    return syscall(SYS_mount, source, target, filesystemtype, data);
}

int 
sys_umount(const char *target) {
    return syscall(SYS_umount, target);
}

#else
#warning ARM use different syscall method

#define __sys2(x) #x
#define __sys1(x) __sys2(x)
 
#ifndef __syscall
#define __syscall(name)  "swi\t" __sys1(SYS_##name) "\n\t"
#endif

#define __syscall_return(type, res)                                      \
do {                                                                              \
return (type) (res);                                                 \
} while (0)

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3)           \
type sys_##name(type1 arg1,type2 arg2,type3 arg3) {                                  \
  long __res;                                                                     \
  __asm__ __volatile__ (                                                        \
  "mov\tr0,%1\n\t"                                                                  \
  "mov\tr1,%2\n\t"                                                                  \
  "mov\tr2,%3\n\t"                                                                  \
  __syscall(name)                                                            \
  "mov\t%0,r0"                                                                         \
        : "=r" (__res)                                                             \
        : "r" ((long)(arg1)),"r" ((long)(arg2)),"r" ((long)(arg3))     \
        : "r0","r1","r2","lr");                                                       \
  __syscall_return(type,__res);                                                     \
}

#define _syscall0(type,name)           \
type sys_##name() {                                  \
  long __res;                                                                     \
  __asm__ __volatile__ (                                                        \
  __syscall(name)                                                            \
  "mov\t%0,r0"                                                                         \
        : "=r" (__res)                                                             \
        :: "r0","lr");                                                       \
  __syscall_return(type,__res);                                                     \
}

#define _syscall1(type,name,type1,arg1)           \
type sys_##name(type1 arg1) {                                  \
  long __res;                                                                     \
  __asm__ __volatile__ (                                                        \
  "mov\tr0,%1\n\t"                                                                  \
  __syscall(name)                                                            \
  "mov\t%0,r0"                                                                         \
        : "=r" (__res)                                                             \
        : "r" ((long)(arg1))     \
        : "r0","lr");                                                       \
  __syscall_return(type,__res);                                                     \
}

#define _syscall2(type,name,type1,arg1,type2,arg2)           \
type sys_##name(type1 arg1,type2 arg2) {                                  \
  long __res;                                                                     \
  __asm__ __volatile__ (                                                        \
  "mov\tr0,%1\n\t"                                                                  \
  "mov\tr1,%2\n\t"                                                                  \
  __syscall(name)                                                            \
  "mov\t%0,r0"                                                                         \
        : "=r" (__res)                                                             \
        : "r" ((long)(arg1)),"r" ((long)(arg2))     \
        : "r0","r1","lr");                                                       \
  __syscall_return(type,__res);                                                     \
}


#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4)           \
type sys_##name(type1 arg1,type2 arg2,type3 arg3, type4 arg4) {                                  \
  long __res;                                                                     \
  __asm__ __volatile__ (                                                        \
  "mov\tr0,%1\n\t"                                                                  \
  "mov\tr1,%2\n\t"                                                                  \
  "mov\tr2,%3\n\t"                                                                  \
  "mov\tr3,%4\n\t"                                                                  \
  __syscall(name)                                                            \
  "mov\t%0,r0"                                                                         \
        : "=r" (__res)                                                             \
        : "r" ((long)(arg1)),"r" ((long)(arg2)),"r" ((long)(arg3)),"r"((long)(arg4))     \
        : "r0","r1","r2","r3","lr");                                                       \
  __syscall_return(type,__res);                                                     \
}


_syscall1(int, exit,int , error);
_syscall0(int,fork);
_syscall2(int, wait,int , pid, int *, store);
_syscall3(int, exec,const char *, filename, const char **, argv,
    const char **, envp);
_syscall0(int,yield);
_syscall1(int, sleep,unsigned int , time);
_syscall1(int, kill,int , pid);
_syscall0(size_t,gettime);
_syscall0(int,getpid);
_syscall1(int, brk,uintptr_t *, brk);
_syscall3(int, mmap,uintptr_t *, addr, size_t , len, uint32_t , mmap);
_syscall2(int, munmap,uintptr_t , addr, size_t , len);
_syscall3(int, shmem,uintptr_t *, addr, size_t , len, uint32_t , mmap);
_syscall1(int, putc,int , c);
_syscall0(int,pgdir);
_syscall1(sem_t, sem_init,int , value);
_syscall1(int, sem_post,sem_t , sem);
_syscall2(int, sem_wait,sem_t , sem, unsigned int , timeout);
_syscall1(int, sem_free,sem_t , sem);
_syscall2(int, sem_get_value,sem_t , sem, int *, value);
_syscall3(int, event_send,int , pid, int , event, unsigned int , timeout);
_syscall3(int, event_recv,int *, pid, int *, event, unsigned int , timeout);
_syscall1(int, mbox_init,unsigned int , max);
_syscall3(int, mbox_send,int , id, struct mboxbuf *, buf, unsigned int , timeout);
_syscall3(int, mbox_recv,int , id, struct mboxbuf *, buf, unsigned int , timeout);
_syscall1(int, mbox_free,int , id);
_syscall2(int, mbox_info,int , id, struct mboxinfo *, info);
_syscall2(int, open,const char *, path, uint32_t , open);
_syscall1(int, close,int , fd);
_syscall3(int, read,int , fd, void *, base, size_t , len);
_syscall3(int, write,int , fd, void *, base, size_t , len);
_syscall3(int, seek,int , fd, off_t , pos, int , whence);
_syscall2(int, fstat,int , fd, struct stat *, stat);
_syscall1(int, fsync,int , fd);
_syscall1(int, chdir,const char *, path);
_syscall2(int, getcwd,char *, buffer, size_t , len);
_syscall1(int, mkdir,const char *, path);
_syscall2(int, link,const char *, path1, const char *, path2);
_syscall2(int, rename,const char *, path1, const char *, path2);
_syscall1(int, unlink,const char *, path);
_syscall2(int, getdirentry,int , fd, struct dirent *, dirent);
_syscall2(int, dup,int , fd1, int , fd2);
_syscall1(int, pipe,int *, fd);
_syscall2(int, mkfifo,const char *, name, uint32_t , open);
_syscall3(int, ioctl, int, d, int, request, unsigned long, data);
_syscall4(void*, linux_mmap, void*, addr, size_t, length, int, fd, size_t, offset);
_syscall2(int, linux_tkill, int ,pid, int,sign);
_syscall2(int, linux_kill, int ,pid, int,sign);
_syscall3(int, linux_sigprocmask,int,how, const sigset_t*, set, sigset_t*, old);
_syscall1(int, linux_sigsuspend, unsigned int, mask);
_syscall3(int, linux_sigaction, int, sign, struct sigaction* ,act,struct sigaction *, old);
_syscall0(int, list_module);
_syscall3(int, init_module, void*, umod, unsigned long, len, const char *, uargs);
_syscall1(int, cleanup_module, const char *, name);
_syscall4(int, mount, const char *, source, const char *, target, const char *, filesystemtype, const void *, data);
_syscall1(int, umount, const char *, target);

int
sys_send_event(int pid, int event, unsigned int timeout) {
    return sys_event_send( pid, event, timeout);
}

int
sys_recv_event(int *pid_store, int *event_store, unsigned int timeout) {
    return sys_event_recv( pid_store, event_store, timeout);
}

#endif

