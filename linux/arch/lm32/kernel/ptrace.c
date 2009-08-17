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

/* struct task_struct */
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/user.h>
#include <linux/signal.h>

#include <asm/registers.h>
#include <asm/uaccess.h>

void ptrace_disable(struct task_struct *child)
{
	/* nothing todo - we have no single step */
}

/*
 * Read a register set.
 */
int ptrace_getregs (struct task_struct *child, unsigned long __user *data)
{
	struct pt_regs *regs;
	int i;

	if (!access_ok(VERIFY_WRITE, data, (32) * 4))
		return -EIO;

	regs = task_pt_regs(child);

	for (i = 0; i < 32; i++)
		__put_user (*((unsigned long*)regs + i), data + i);
	/* special case: sp: we always want to get the USP! */
	__put_user (current->thread.usp, data + i);

	return 0;
}

/*
 * Write a register set.
 */
int ptrace_setregs (struct task_struct *child, unsigned long __user *data)
{
	struct pt_regs *regs;
	int i;
	unsigned long tmp;

	if (!access_ok(VERIFY_READ, data, 32 * 4))
		return -EIO;

	regs = task_pt_regs(child);

	for (i = 0; i < 32; i++)
		__get_user (*((unsigned long*)regs + i), data + i);
	/* special case: sp: we always want to set the USP! */
	__get_user (tmp, data + 28);
	child->thread.usp = tmp;

	return 0;
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
	int ret;

	//printk("arch_ptrace: %lx %lx %lx %lx\n", child, request, addr, data);
	switch (request) {
	/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */
	case PTRACE_PEEKDATA: {
		unsigned long tmp;
		int copied;

		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		ret = -EIO;
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp,(unsigned long __user *) data);
		//printk("PTRACE_PEEK* [%lx] @%lx = %lx\n", child, addr, tmp);
		break;
	}

	/* Read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR: {
		struct pt_regs *regs;
		unsigned long tmp = 0;

		regs = task_pt_regs(child);
		ret = 0;  /* Default return value. */

		switch (addr) {
		case 0 ... 27:
		case 29 ... 31: {
			unsigned long* pregs = (unsigned long*) regs;
			pregs += addr;
			tmp = *pregs; }
			break;
		case 28: { /* sp */
			/* special case: sp: we always want to get the USP! */
			tmp = child->thread.usp; }
			break;
		case PT_TEXT_ADDR:
			tmp = child->mm->start_code;
			break;
		case PT_TEXT_END_ADDR:
			tmp = child->mm->end_code;
			break;
		case PT_DATA_ADDR:
			tmp = child->mm->start_data;
			break;
		default:
			tmp = 0;
			ret = -EIO;
			printk("ptrace attempted to PEEKUSR at %lx\n", addr);
			goto out;
		}
		ret = put_user(tmp, (unsigned long __user *) data);
		//printk("PTRACE_PEEKUSR [%s] 0x%lx+%d = %lx\n", child->comm, regs, addr, tmp);
		break;
	}

	/* when I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		//printk("PTRACE_POKE* [%s] *0x%lx = 0x%lx\n", child->comm, addr, data);
		if (access_process_vm(child, addr, &data, sizeof(data), 1)
		    == sizeof(data)) {
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("wcsr ICC, r0");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("wcsr DCC, r0");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			break;
		}
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: {
		struct pt_regs *regs;
		ret = 0;
		regs = task_pt_regs(child);

		switch (addr) {
		case 0 ... 27:
		case 29 ... 31: {
			unsigned long* pregs = (unsigned long*) regs;
			pregs += addr;
			*pregs = data; }
			break;
		case 28: { /* sp */
			/* special case: sp: we always want to set the USP! */
			child->thread.usp = data; }
			break;
		default:
			/* The rest are not allowed. */
			ret = -EIO;
			printk("ptrace attempted to POKEUSR at %lx\n", addr);
			break;
		}
		break;
		}

	case PTRACE_GETREGS:
		ret = ptrace_getregs (child, (unsigned long __user *) data);
		break;

	case PTRACE_SETREGS:
		ret = ptrace_setregs (child, (unsigned long __user *) data);
		break;

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_SINGLESTEP: /* Execute a single instruction. */
	case PTRACE_CONT: { /* restart after signal. */
		ret = -EIO;
		if (!valid_signal(data))
			break;
		if (request == PTRACE_SYSCALL) {
			set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		}
		else {
			clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		}
		child->exit_code = data;
		wake_up_process(child);
		ret = 0;
		break;
	}

	/*
	 * make the child exit.  Best I can do is send it a sigkill.
	 * perhaps it should be put in the status that it wants to
	 * exit.
	 */
	case PTRACE_KILL:
		ret = 0;
		if (child->exit_state == EXIT_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		wake_up_process(child);
		break;

	case PTRACE_DETACH: /* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	/* PTRACE_GET_THREAD_AREA */
	default:
		printk("warning: ptrace default request %lx %lx %lx %lx\n", (unsigned long)child, request, addr, data);
		ret = ptrace_request(child, request, addr, data);
		break;
	}
 out:
	return ret;
}

