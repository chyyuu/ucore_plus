#include <types.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <arch.h>
#include <stdio.h>
#include <kdebug.h>
#include <assert.h>
#include <sync.h>
#include <monitor.h>
#include <console.h>
#include <vmm.h>
#include <proc.h>
#include <sched.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <kio.h>
#include <mp.h>

#define TICK_NUM 30

static struct gatedesc idt[256] = { {0} };

static struct pseudodesc idt_pd = {
	sizeof(idt) - 1, (uintptr_t) idt
};

void idt_init(void)
{
	extern uintptr_t __vectors[];
	int i;
	for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i++) {
		SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
	}
	SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
	lidt(&idt_pd);
}

static const char *trapname(int trapno)
{
	static const char *const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames) / sizeof(const char *const)) {
		return excnames[trapno];
	}
	if (trapno == T_SYSCALL) {
		return "System call";
	}
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
		return "Hardware Interrupt";
	}
	return "(unknown trap)";
}

bool trap_in_kernel(struct trapframe * tf)
{
	return (tf->tf_cs == (uint16_t) KERNEL_CS);
}

static const char *IA32flags[] = {
	"CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
	"TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
	"RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void print_trapframe(struct trapframe *tf)
{
	kprintf("trapframe at %p\n", tf);
	print_regs(&tf->tf_regs);
	kprintf("  es   0x----%04x\n", tf->tf_es);
	kprintf("  ds   0x----%04x\n", tf->tf_ds);
	kprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	kprintf("  err  0x%08x\n", tf->tf_err);
	kprintf("  eip  0x%08x\n", tf->tf_eip);
	kprintf("  cs   0x----%04x\n", tf->tf_cs);
	kprintf("  flag 0x%08x ", tf->tf_eflags);

	int i, j;
	for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]);
	     i++, j <<= 1) {
		if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
			kprintf("%s,", IA32flags[i]);
		}
	}
	kprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

	if (!trap_in_kernel(tf)) {
		kprintf("  esp  0x%08x\n", tf->tf_esp);
		kprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void print_regs(struct pushregs *regs)
{
	kprintf("  edi  0x%08x\n", regs->reg_edi);
	kprintf("  esi  0x%08x\n", regs->reg_esi);
	kprintf("  ebp  0x%08x\n", regs->reg_ebp);
	kprintf("  oesp 0x%08x\n", regs->reg_oesp);
	kprintf("  ebx  0x%08x\n", regs->reg_ebx);
	kprintf("  edx  0x%08x\n", regs->reg_edx);
	kprintf("  ecx  0x%08x\n", regs->reg_ecx);
	kprintf("  eax  0x%08x\n", regs->reg_eax);
}

static inline void print_pgfault(struct trapframe *tf)
{
	/* error_code:
	 * bit 0 == 0 means no page found, 1 means protection fault
	 * bit 1 == 0 means read, 1 means write
	 * bit 2 == 0 means kernel, 1 means user
	 * */
	kprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
		(tf->tf_err & 4) ? 'U' : 'K',
		(tf->tf_err & 2) ? 'W' : 'R',
		(tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int pgfault_handler(struct trapframe *tf)
{
	extern struct mm_struct *check_mm_struct;
	struct mm_struct *mm;
	if (check_mm_struct != NULL) {
		assert(current == idleproc);
		mm = check_mm_struct;
	} else {
		if (current == NULL) {
			print_trapframe(tf);
			print_pgfault(tf);
			panic("unhandled page fault.\n");
		}
		mm = current->mm;
	}
	return do_pgfault(mm, tf->tf_err, rcr2());
}

static void trap_dispatch(struct trapframe *tf)
{
	char c;

	int ret;
	switch (tf->tf_trapno) {
	case T_DEBUG:
	case T_BRKPT:
		debug_monitor(tf);
		break;
	case T_PGFLT:
		if ((ret = pgfault_handler(tf)) != 0) {
			print_trapframe(tf);
			if (current == NULL) {
				panic("handle pgfault failed. %e\n", ret);
			} else {
				if (trap_in_kernel(tf)) {
					panic
					    ("handle pgfault failed in kernel mode. %e\n",
					     ret);
				}
				kprintf("killed by kernel.\n");
				do_exit(-E_KILLED);
			}
		}
		break;
	case T_SYSCALL:
		syscall();
		break;
	case IRQ_OFFSET + IRQ_TIMER:
		ticks++;
		assert(current != NULL);
		run_timer_list();
		break;
	case IRQ_OFFSET + IRQ_COM1:
	case IRQ_OFFSET + IRQ_KBD:
		c = cons_getc();

		extern void dev_stdin_write(char c);
		dev_stdin_write(c);
		break;
	case IRQ_OFFSET + IRQ_IDE1:
	case IRQ_OFFSET + IRQ_IDE2:
		/* do nothing */
		break;
	default:
		print_trapframe(tf);
		if (current != NULL) {
			kprintf("unhandled trap.\n");
			do_exit(-E_KILLED);
		}
		panic("unexpected trap in kernel.\n");
	}
}

void trap(struct trapframe *tf)
{
	// used for previous projects
	if (current == NULL) {
		trap_dispatch(tf);
	} else {
		// keep a trapframe chain in stack
		struct trapframe *otf = current->tf;
		current->tf = tf;

		bool in_kernel = trap_in_kernel(tf);

		trap_dispatch(tf);

		current->tf = otf;
		if (!in_kernel) {
			may_killed();
			if (current->need_resched) {
				schedule();
			}
		}
	}
}

int ucore_in_interrupt()
{
	return 0;
}
