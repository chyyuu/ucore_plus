#ifndef __LIBS_UNISTD_H__
#define __LIBS_UNISTD_H__

#define T_SYSCALL           0x80

#define __SYS_linux         0

/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_exit_thread     9
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
#define SYS_sem_init        40
#define SYS_sem_post        41
#define SYS_sem_wait        42
#define SYS_sem_free        43
#define SYS_sem_get_value   44
#define SYS_event_send      48
#define SYS_event_recv      49
#define SYS_mbox_init       50
#define SYS_mbox_send       51
#define SYS_mbox_recv       52
#define SYS_mbox_free       53
#define SYS_mbox_info       54
#define SYS_open            100
#define SYS_close           101
#define SYS_read            102
#define SYS_write           103
#define SYS_seek            104
#define SYS_fstat           110
#define SYS_fsync           111
#define SYS_chdir           120
#define SYS_getcwd          121
#define SYS_mkdir           122
#define SYS_link            123
#define SYS_rename          124
#define SYS_readlink        125
#define SYS_symlink         126
#define SYS_unlink          127
#define SYS_getdirentry     128
#define SYS_dup             130
#define SYS_pipe            140
#define SYS_mkfifo          141


/* chenyh 2012/7 */
#define SYS_ioctl           142
#define SYS_linux_mmap      143
#define SYS_linux_tkill     144
#define SYS_linux_sigaction 145
#define SYS_linux_kill      146
#define SYS_linux_sigprocmask   147
#define SYS_linux_sigsuspend   148
#define SYS_linux_sigreturn    149

/* changlan */
#define SYS_init_module     150
#define SYS_cleanup_module  151
#define SYS_list_module     152
#define SYS_mount           153
#define SYS_umount			154

/* SYS_fork flags */
#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_PTRACE         0x00002000
#define CLONE_VFORK          0x00004000
#define CLONE_PARENT         0x00008000
#define CLONE_THREAD         0x00010000
#define CLONE_NEWNS          0x00020000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_DETACHED       0x00400000
#define CLONE_UNTRACED       0x00800000
#define CLONE_CHILD_SETTID   0x01000000
#define CLONE_STOPPED        0x02000000
//FIXME
#define CLONE_SEM            CLONE_THREAD


/* SYS_mmap flags */
#define MMAP_WRITE          0x00000100
#define MMAP_STACK          0x00000200

#if 0
/* VFS flags */
// flags for open: choose one of these
#define O_RDONLY            0           // open for reading only
#define O_WRONLY            1           // open for writing only
#define O_RDWR              2           // open for reading and writing
// then or in any of these:
#define O_CREAT             0x00000004  // create file if it does not exist
#define O_EXCL              0x00000008  // error if O_CREAT and the file exists
#define O_TRUNC             0x00000010  // truncate file upon open
#define O_APPEND            0x00000020  // append on each write
// additonal related definition
#define O_ACCMODE           3           // mask for O_RDONLY / O_WRONLY / O_RDWR

#endif
#define NO_FD               -0x9527     // invalid fd

/* open/fcntl - O_SYNC is only implemented on blocks devices and on files
   located on an ext2 file system */
#define O_ACCMODE          0003
#define O_RDONLY             00
#define O_WRONLY             01
#define O_RDWR               02
#define O_CREAT            0100 /* not fcntl */
#define O_EXCL             0200 /* not fcntl */
#define O_NOCTTY           0400 /* not fcntl */
#define O_TRUNC           01000 /* not fcntl */
#define O_APPEND          02000
#define O_NONBLOCK        04000
#define O_NDELAY        O_NONBLOCK
#define O_SYNC           010000
#define O_FSYNC          O_SYNC
#define O_ASYNC          020000


/* lseek codes */
#define LSEEK_SET           0           // seek relative to beginning of file
#define LSEEK_CUR           1           // seek relative to current position in file
#define LSEEK_END           2           // seek relative to end of file

#define FS_MAX_DNAME_LEN    31
#define FS_MAX_FNAME_LEN    255
#define FS_MAX_FPATH_LEN    4095

#define EXEC_MAX_ARG_NUM    32
#define EXEC_MAX_ENV_NUM    32
#define EXEC_MAX_ARG_LEN    4095
#define EXEC_MAX_ENV_LEN    4095

#endif /* !__LIBS_UNISTD_H__ */

