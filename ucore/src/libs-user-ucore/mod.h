#ifndef __USER_LIBS_MOD_H__
#define __USER_LIBS_MOD_H__

#ifndef __user
#define __user
#endif

int init_module(void __user *umod, unsigned long len, const char __user * uargs);
int cleanup_module(const char __user *name);

#endif

