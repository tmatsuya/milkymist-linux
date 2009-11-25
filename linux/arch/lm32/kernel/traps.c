/*
 * (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Sets up all exception vectors
 */

#include <linux/init.h>
#include <linux/sched.h> /* struct task_struct */
#include <linux/kallsyms.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/hardirq.h>
#include <linux/ptrace.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/registers.h>
#include <asm/traps.h>

extern unsigned long  reset_handler;

void __init trap_init(void)
{
  void*  exception_vectors = &reset_handler;

	if( ((unsigned long)&reset_handler % 256) != 0 )
		printk(KERN_ERR "exception vectors are not aligned to 256 bytes!\n");
  asm volatile ( "wcsr EBA, %0" : : "r"(exception_vectors) );
#ifndef LM32_HW_JTAG
	// set DEBA so that we can catch breakpoints in linux
  asm volatile ( "wcsr DEBA, %0" : : "r"(exception_vectors) );
#endif
}

int kstack_depth_to_print = 48;

static void show_trace(struct task_struct *tsk, unsigned long *sp)
{
	unsigned long addr;

	asm volatile ("break");

	printk("\nCall Trace:");
#ifdef CONFIG_KALLSYMS
	printk("\n");
#endif

	while (!kstack_end(sp)) {
		addr = *sp++;
		/*
		 * If the address is either in the text segment of the
		 * kernel, or in the region which contains vmalloc'ed
		 * memory, it *may* be the address of a calling
		 * routine; if so, print it so that someone tracing
		 * down the cause of the crash will be able to figure
		 * out the call path that was taken.
		 */
		if (kernel_text_address(addr))
			print_ip_sym(addr);
	}

	printk("\n");
}

void dump_stack(void)
{
	unsigned long dummy;
	show_trace(NULL, &dummy);
}
EXPORT_SYMBOL(dump_stack);

void show_stack(struct task_struct *task, unsigned long *stack)
{
	unsigned long *endstack, addr;
	int i;

	if (!stack) {
		if (task)
			stack = (unsigned long *)task->thread.ksp;
		else
			stack = (unsigned long *)&stack;
	}

	addr = (unsigned long)stack;
	endstack = (unsigned long *)PAGE_ALIGN(addr);

	printk(KERN_EMERG "Stack from %08lx:", (unsigned long)stack);
	for (i = 0; i < kstack_depth_to_print; i++) {
		if (stack + 1 > endstack)
			break;
		if (i % 8 == 0)
			printk("\n" KERN_EMERG "       ");
		printk(" %08lx", *stack++);
	}

	show_trace(task, stack);
}

asmlinkage unsigned long asm_do_sig(unsigned long vec, struct pt_regs *regs)
{
	struct siginfo info;
	struct task_struct *tsk;
	struct mm_struct *mm;
	int sig;
 	int trapidx = 0;
	unsigned long addr;
	//register unsigned long sp asm("sp");

	switch(vec)
	{
		case 32: /* breakpoint */
			addr = regs->ba;
			//printk("Breakpoint @ %lx (%lx)\n", addr, regs->ea);
			//printk("  regs=%lx ksp=%lx usp=%lx sp=%lx\n", regs, current->thread.ksp, current->thread.usp, sp);
			/* keep gdb happy */
			regs->ea = addr;
			sig = SIGTRAP;
 			trapidx = 1;
			break;

		case 96: /* watchpoint */
			addr = regs->ba;
			printk("Watchpoint @ %lx\n", addr);
			sig = SIGTRAP;
 			trapidx = 3;
			break;

		case 64: /* instruction bus error */
			addr = regs->ea;
			printk("Illegal Instruction Error @ %lx\n", addr);
			sig = SIGILL;
 			trapidx = 2;
			break;

		case 128: /* data bus error */
			addr = regs->ea;
			printk("Data Bus Error @ %lx\n", addr);
			sig = SIGSEGV;
 			trapidx = 4;
			break;

		case 160: /* div by zero */
			addr = regs->ea;
			printk("Divide By Zero Error @ %lx\n", addr);
			sig = SIGFPE;
 			trapidx = 5;
			break;

		default: /* unspecified -> segfault */
			addr = regs->ea;
			printk("Unspecified Exception @ %lx/%lx (defaulting to SIGSEGV)\n", addr, regs->ba);
			sig = SIGSEGV;
 			trapidx = 4;
			break;
	}

	memset(&info, 0, sizeof(info));
	info.si_signo = sig;
	info.si_addr = (void*)addr;

	tsk = current;
	mm = tsk->mm;

	/*
	 * If we're on the kernel stack or during disabled interrupts
	 * or in an atomic section or if we have no user context, the kernel gets the fault.
	 */
	if( !mm || in_atomic())
	{
		/* the kernel gets the fault */
		bust_spinlocks(1);
		printk(KERN_ALERT "%s: unhandled kernel space fault (%d) @ %lx\n", tsk->comm, sig, addr);
		bust_spinlocks(0);
		local_irq_enable();
		do_exit(SIGKILL);
		return 0;
	} else {
		/* the user space gets the fault */
		if( sig != SIGTRAP )
			printk(KERN_ERR "%s: unhandled user space fault (%d) @ %lx\n", tsk->comm, sig, addr);
		force_sig_info(sig, &info, tsk);
		return regs->r1;
	}
}

