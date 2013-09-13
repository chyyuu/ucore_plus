
#include "mod.h"
#include <syscall.h>

int init_module(void __user * umod, unsigned long len,
		const char __user * uargs)
{
	return sys_init_module(umod, len, uargs);
}

int cleanup_module(const char __user * name)
{
	return sys_cleanup_module(name);
}

int query_module(const char * name)
{
	return sys_query_module(name);
}
