#include "mount.h"
#include <syscall.h>

int 
mount(const char *source, const char *target, const char *filesystemtype, const void *data) {
	return sys_mount(source, target, filesystemtype, data);
}

int 
umount(const char *target) {
	return sys_umount(target);
}