#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include <types.h>
#ifndef __user
#define __user
#endif

int sys_exit(int error_code);
int sys_fork(void);
int sys_wait(int pid, int *store);
int sys_exec(const char *filename, const char **argv, const char **envp);
int sys_yield(void);
int sys_sleep(unsigned int time);
int sys_kill(int pid);
size_t sys_gettime(void);
int sys_getpid(void);
int sys_brk(uintptr_t * brk_store);
int sys_mmap(uintptr_t * addr_store, size_t len, uint32_t mmap_flags);
int sys_munmap(uintptr_t addr, size_t len);
int sys_shmem(uintptr_t * addr_store, size_t len, uint32_t mmap_flags);
int sys_putc(int c);
int sys_pgdir(void);
sem_t sys_sem_init(int value);
int sys_sem_post(sem_t sem_id);
int sys_sem_wait(sem_t sem_id, unsigned int timeout);
int sys_sem_free(sem_t sem_id);
int sys_sem_get_value(sem_t sem_id, int *value_store);
int sys_send_event(int pid, int event, unsigned int timeout);
int sys_recv_event(int *pid_store, int *event_store, unsigned int timeout);

//#define sys_event_send(x,y,z) sys_send_event(x,y,x)
//#define sys_event_recv(x,y,z) sys_recv_event(x,y,x)

struct mboxbuf;
struct mboxinfo;

int sys_mbox_init(unsigned int max_slots);
int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_free(int id);
int sys_mbox_info(int id, struct mboxinfo *info);

struct stat;
struct dirent;

int sys_open(const char *path, uint32_t open_flags);
int sys_close(int fd);
int sys_read(int fd, void *base, size_t len);
int sys_write(int fd, void *base, size_t len);
int sys_seek(int fd, off_t pos, int whence);
int sys_fstat(int fd, struct stat *stat);
int sys_fsync(int fd);
int sys_chdir(const char *path);
int sys_getcwd(char *buffer, size_t len);
int sys_mkdir(const char *path);
int sys_link(const char *path1, const char *path2);
int sys_rename(const char *path1, const char *path2);
int sys_unlink(const char *path);
int sys_getdirentry(int fd, struct dirent *dirent);
int sys_dup(int fd1, int fd2);
int sys_pipe(int *fd_store);
int sys_mkfifo(const char *name, uint32_t open_flags);

int sys_init_module(void __user * umod, unsigned long len,
		    const char __user * uargs);
int sys_cleanup_module(const char __user * name);
int sys_list_module();

int sys_mount(const char *source, const char *target,
	      const char *filesystemtype, const void *data);
int sys_umount(const char *target);

int sys_mod_add(int, int);
int sys_mod_mul(int, int);

int sys_ioctl(int d, int request, unsigned long data);

void *sys_linux_mmap(void *addr, size_t length, int fd, size_t offset);

int sys_rf212_send(uint8_t len, uint8_t * data);
int sys_rf212_reg(uint8_t reg, uint8_t value);
int sys_rf212_reset();
//halt the system
int sys_halt();
#endif /* !__USER_LIBS_SYSCALL_H__ */
