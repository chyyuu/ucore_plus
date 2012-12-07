#include <proc.h>
#include <assert.h>

int __sig_setup_frame(int sign, struct sigaction *act, sigset_t oldset, struct trapframe *tf)
{
	warn("%s not implemented yet.", __func__);
	return -1;
}
