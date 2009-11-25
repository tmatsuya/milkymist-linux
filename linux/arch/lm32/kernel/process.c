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
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm/setup.h>
#include <asm/pgtable.h>

asmlinkage void ret_from_fork(void);


struct thread_info* lm32_current_thread;

/*
 * The following aren't currently used.
 */
void (*pm_idle)(void);
EXPORT_SYMBOL(pm_idle);

void (*pm_power_off)(void);
EXPORT_SYMBOL(pm_power_off);

/*
 * The idle loop on an LM32
 */
static void default_idle(void)
{
 	while(!need_resched());
}

void (*idle)(void) = default_idle;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	while (1) {
		idle();
		preempt_enable_no_resched();
		schedule();
		preempt_disable();
	}
}

void machine_restart(char * __unused)
{
	asm volatile(
		"wcsr IE, r0\n"
		"wcsr IM, r0\n"
		"wcsr EBA, r0\n"
		"rcsr r1, IP\n"
		"wcsr IP, r1\n"
		"nop\n"
		"nop\n"
#if defined(CONFIG_PLAT_MILKYMIST)
		"mvi r1, 0x100\n"
		"call r1\n"
#else
		"call r0\n"
#endif
		"nop\n"
	);
}

void machine_halt(void)
{
	printk("%s:%d: machine_halt() is not possible on lm32\n", __FILE__, __LINE__);
	for (;;)
		cpu_relax();
}

void machine_power_off(void)
{
	printk("%s:%d: machine_poweroff() is not possible on lm32\n", __FILE__, __LINE__);
	for (;;)
		cpu_relax();
}

void show_regs(struct pt_regs * regs)
{
	printk("Registers:\n");
	#define LM32REG(name) printk("%3s : 0x%lx\n", #name, regs->name)
	LM32REG(r0);  LM32REG(r1);  LM32REG(r2);  LM32REG(r3);  LM32REG(r4);
	LM32REG(r5);  LM32REG(r6);  LM32REG(r7);  LM32REG(r8);  LM32REG(r9);
	LM32REG(r10); LM32REG(r11); LM32REG(r12); LM32REG(r13); LM32REG(r14);
	LM32REG(r15); LM32REG(r16); LM32REG(r17); LM32REG(r18); LM32REG(r19);
	LM32REG(r20); LM32REG(r21); LM32REG(r22); LM32REG(r23); LM32REG(r24);
	LM32REG(r25); LM32REG(gp);  LM32REG(fp);  LM32REG(sp);  LM32REG(ra);
	LM32REG(ea);  LM32REG(ba);
	#undef LM32REG
}


void kernel_thread_helper(int reserved, int (*fn)(void*), void* arg)
{
  /* Note: read copy_thread, kernel_thread and ret_from_fork to fully appreciate why the first argument is "reserved" */

	do_exit(fn(arg));
}

/*
 * Create a kernel thread
 */
int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	/* prepare registers from which a child task switch frame will be copied */
	struct pt_regs regs;

	set_fs(KERNEL_DS);

	memset(&regs, 0, sizeof(regs));

	//printk("kernel_thread fn=%x arg=%x regs=%x\n", fn, arg, &regs);

	regs.r2 = (unsigned long)fn;
	regs.r3 = (unsigned long)arg;
	regs.r5 = (unsigned long)kernel_thread_helper;
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs, 0, NULL, NULL);
}

void flush_thread(void)
{
	set_fs(USER_DS);
}


/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(char *name, char **argv, char **envp, struct pt_regs* regs)
{
	int error;
	char * filename;

	lock_kernel();
	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	unlock_kernel();
	return error;
}

/* no stack unwinding */
unsigned long get_wchan(struct task_struct *p)
{
	return 0;
}

unsigned long thread_saved_pc(struct task_struct *tsk)
{
	return 0;
}



int copy_thread(int nr, unsigned long clone_flags,
		unsigned long usp, unsigned long stk_size,
		struct task_struct * p, struct pt_regs * regs)
{
	unsigned long child_tos = KSTK_TOS(p);
	struct pt_regs *childregs;

	if (regs->r5 == (unsigned long)kernel_thread_helper) {
		/* kernel thread */

		if( usp != 0 )
			panic("trying to start kernel thread with usp != 0");

		/* childregs = full task switch frame on kernel stack of child */
		childregs = (struct pt_regs *)(child_tos) - 1;

		*childregs = *regs;

		childregs->r4 = 0; /* child gets zero as return value */
		regs->r4 = p->pid; /* parent gets child pid as return value */ 

		/* return via ret_from_fork */
		childregs->ra = (unsigned long)ret_from_fork;

		/* setup ksp/usp */
		p->thread.ksp = (unsigned long)childregs - 4; /* perhaps not necessary */
		childregs->sp = p->thread.ksp;
		p->thread.usp = 0;
		p->thread.which_stack = 0; /* kernel stack */

		//printk("copy_thread1: ->pid=%d tsp=%lx r5=%lx p->thread.ksp=%lx p->thread.usp=%lx\n",
		//		p->pid, task_stack_page(p), childregs->r5, p->thread.ksp, p->thread.usp);
	} else {
		/* userspace thread (vfork, clone) */

		unsigned long ra_in_syscall;
		struct pt_regs* childsyscallregs;

		//asm volatile("break");

		/* this was brought to us by sys_lm32_vfork */
		ra_in_syscall = regs->r1;

		/* childsyscallregs = full syscall frame on kernel stack of child */
		childsyscallregs = (struct pt_regs *)(child_tos) - 1; /* 32 = safety */

		/* childregs = full task switch frame on kernel stack of child below * childsyscallregs */
		childregs = childsyscallregs - 1;

		/* child shall have same syscall context to restore as parent has ... */
		*childsyscallregs = *regs;
		/* no need to set return value here, it will be set by task switch frame */

		/* copy task switch frame, child shall return with the same registers as parent
		 * entered the syscall except for return value of syscall */
		*childregs = *regs;

		regs->r4 = p->pid; /* parent gets child pid as return value */ 

		/* user stack pointer is shared with the parent per definition of vfork */
		p->thread.usp = usp;

		/* kernel stack pointer is not shared with parent, it is the beginning of
		 * the just created new task switch segment on the kernel stack */
		p->thread.ksp = (unsigned long)childregs - 4;
		p->thread.which_stack = 0; /* resume from ksp */

		/* child returns via ret_from_fork */
		childregs->ra = (unsigned long)ret_from_fork;
		/* child shall return to where sys_vfork_wrapper has been called */
		childregs->r5 =	ra_in_syscall;
		/* child gets zero as return value from syscall */
		childregs->r4 = 0;
		/* after task switch segment return the stack pointer shall point to the
		 * syscall frame */
		childregs->sp = (unsigned long)childsyscallregs - 4;

		put_task_struct(p);

		/*printk("copy_thread2: ->pid=%d p=%lx regs=%lx childregs=%lx r5=%lx ra=%lx "
				"dsf=%lx p->thread.ksp=%lx p->thread.usp=%lx\n",
				p->pid, p, regs, childregs, childregs->r5, childregs->ra,
				dup_syscallframe, p->thread.ksp, p->thread.usp);*/
	}

	return 0;
}

/* start userspace thread */
void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long usp)
{
	unsigned long *stack;

	set_fs(USER_DS);

	memset(regs, 0, sizeof(regs));

	stack = (unsigned long *)usp;
	/* -4 because we will add 4 later in ret_from_syscall */
	regs->ea = pc - 4;
	regs->r1 = stack[0];
	regs->r2 = stack[1];
	regs->r3 = stack[2];
	regs->r7 = current->mm->context.exec_fdpic_loadmap;
	regs->sp = usp;
	current->thread.usp = usp;
	regs->fp = current->mm->start_data;

	//printk("start_thread: current=%lx usp=%lx\n", current, usp);
}

