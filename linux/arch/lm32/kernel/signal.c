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
 * Based on
 *
 *  arch/v850/kernel/signal.c
 *  Copyright (C) 2001,02,03  NEC Electronics Corporation
 *  Copyright (C) 2001,02,03  Miles Bader <miles@gnu.org>
 *  Copyright (C) 1999,2000,2002  Niibe Yutaka & Kaz Kojima
 *  Copyright (C) 1991,1992  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * 1997-11-28  Modified for POSIX.1b signals by Richard Henderson
 */

#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <linux/tty.h>
#include <linux/hardirq.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/thread_info.h>
#include <asm/cacheflush.h>

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage int manage_signals(int retval, struct pt_regs* regs);

int do_signal(int retval, struct pt_regs *regs, int* handled);

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 */

asmlinkage int
sys_rt_sigsuspend(sigset_t *unewset, size_t sigsetsize, struct pt_regs *regs)
{
	sigset_t saveset, newset;

	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&newset, unewset, sizeof(newset)))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	while (1) {
		int handled = 0;
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		do_signal(-ERESTARTNOHAND, regs, &handled);
		if( handled )
			return -EINTR;
	}
}

asmlinkage int 
sys_sigaction(int sig, const struct old_sigaction *act,
	      struct old_sigaction *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}

asmlinkage int
sys_sigaltstack(const stack_t *uss, stack_t *uoss,
		struct pt_regs *regs)
{
	return do_sigaltstack(uss, uoss, current->thread.usp);
}

/*
 * Do a signal return; undo the signal stack.
 */

struct sigframe
{
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS-1];
	unsigned long tramp[2];	/* signal trampoline */
};

static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext *sc, int *rval_p)
{
	unsigned int err = 0;

#define COPY(x)		err |= __get_user(regs->x, &sc->regs.x)
	COPY(r0);	COPY(r1);	COPY(r2);	COPY(r3);
	COPY(r4);	COPY(r5);	COPY(r6);	COPY(r7);
	COPY(r8);	COPY(r9);	COPY(r10);	COPY(r11);
	COPY(r12);	COPY(r13);	COPY(r14);	COPY(r15);
	COPY(r16);	COPY(r17);	COPY(r18);	COPY(r19);
	COPY(r20);	COPY(r21);	COPY(r22);	COPY(r23);
	COPY(r24);	COPY(r25);	COPY(gp);	COPY(fp);
	COPY(sp);	COPY(ra);	COPY(ea);	COPY(ba);
#undef COPY

	*rval_p = regs->r1;

	return err;
}

asmlinkage int sys_sigreturn(struct pt_regs *regs)
{
	struct sigframe *frame = (struct sigframe *)(current->thread.usp+4);
	sigset_t set;
	int rval = 0;

#if DEBUG_SIG
	printk("SIGRETURN\n");
#endif

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__get_user(set.sig[0], &frame->sc.oldmask)
	    || (_NSIG_WORDS > 1
		&& __copy_from_user(&set.sig[1], &frame->extramask,
				    sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->sc, &rval))
		goto badframe;
	current->thread.usp = regs->sp;
	return rval;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

/*
 * Set up a signal frame.
 */
static int
setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs,
		 unsigned long mask)
{
	int err = 0;

#define COPY(x)		err |= __put_user(regs->x, &sc->regs.x)
	COPY(r0);	COPY(r1);	COPY(r2);	COPY(r3);
	COPY(r4);	COPY(r5);	COPY(r6);	COPY(r7);
	COPY(r8);	COPY(r9);	COPY(r10);	COPY(r11);
	COPY(r12);	COPY(r13);	COPY(r14);	COPY(r15);
	COPY(r16);	COPY(r17);	COPY(r18);	COPY(r19);
	COPY(r20);	COPY(r21);	COPY(r22);	COPY(r23);
	COPY(r24);	COPY(r25);	COPY(gp);	COPY(fp);
	COPY(sp);	COPY(ra);	COPY(ea);	COPY(ba);
#undef COPY

	err |= __put_user(mask, &sc->oldmask);

	return err;
}

/*
 * Determine which stack to use..
 */
static inline void *
get_sigframe(struct k_sigaction *ka, struct pt_regs *regs, size_t frame_size)
{
	/* Per default use user stack of userspace process */
	unsigned long sp = current->thread.usp;

	if ((ka->sa.sa_flags & SA_ONSTACK) != 0 && ! sas_ss_flags(sp))
		/* use stack set by sigaltstack */
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void *)((sp - frame_size) & -8UL);
}

static int setup_frame(int sig, struct k_sigaction *ka,
			sigset_t *set, struct pt_regs *regs)
{
	struct sigframe *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	regs->sp = current->thread.usp;
	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);

	if (_NSIG_WORDS > 1) {
		err |= __copy_to_user(frame->extramask, &set->sig[1],
				      sizeof(frame->extramask));
	}

	/* Set up to return from userspace. */

	/* mvi  r8, __NR_sigreturn = addi  r8, r0, __NR_sigreturn */
	err |= __put_user(0x34080000 | __NR_sigreturn, &frame->tramp[0]);

	/* scall */
	err |= __put_user(0xac000007, &frame->tramp[1]);

	if (err)
		goto give_sigsegv;
	
	/* flush instruction cache */
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("wcsr ICC, r0");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");

	/* set return address for signal handler to trampoline */
	regs->ra = (unsigned long)(&frame->tramp[0]);

	/* Set up registers for returning to signal handler */
	/* entry point */
	regs->ea = (unsigned long)ka->sa.sa_handler - 4;
	/* stack pointer */
	regs->sp = (unsigned long)frame - 4;
	current->thread.usp = regs->sp;
	/* Signal handler arguments */
	regs->r1 = signal;     /* first argument = signum */
	regs->r2 = (unsigned long)&frame->sc; /* second argument = sigcontext */

	set_fs(USER_DS);

#if DEBUG_SIG
	printk("SIG deliver (%s:%d): frame=%p, sp=%p ra=%08lx ea=%08lx, signal(r1)=%d\n",
	       current->comm, current->pid, frame, regs->sp, regs->ra, regs->ea, signal);
#endif

	return regs->r1;

give_sigsegv:
	force_sigsegv(sig, current);

	return -1;
}

/*
 * OK, we're invoking a handler
 */	

static int
handle_signal(int retval, unsigned long sig, siginfo_t *info, struct k_sigaction *ka,
	      sigset_t *oldset,	struct pt_regs * regs)
{
	/* Are we from a system call? */
	if (regs->r8) {
		/* return from signal with no error per default */
		regs->r1 = 0;

		/* If so, check system call restarting.. */
		switch (retval) {
		case -ERESTART_RESTARTBLOCK:
			current_thread_info()->restart_block.fn =
				do_no_restart_syscall;
			/* fall through */
		case -ERESTARTNOHAND:
			regs->r1 = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->r1 = -EINTR;
				break;
			}
			/* fallthrough */
		case -ERESTARTNOINTR:
			regs->ea -= 4; /* Size of scall insn.  */
		}
	}

	/* Set up the stack frame */
	if (ka->sa.sa_flags & SA_SIGINFO)
		printk(KERN_ERR "SA_SIGINFO not supported!");
	else
		retval = setup_frame(sig, ka, oldset, regs);

	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked,sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	return retval;
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
int do_signal(int retval, struct pt_regs *regs, int* handled)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;
	sigset_t *oldset;

	if( handled )
		*handled = 1;

	/*
	 * We want the common case to go fast, which
	 * is why we may in certain cases get here from
	 * kernel mode. Just return without doing anything
	 * if so.
	 */
	if (!user_mode(regs))
		return retval;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Whee!  Actually deliver the signal.  */
		return handle_signal(retval, signr, &info, &ka, oldset, regs);
	}

	/* Did we come from a system call? */
	if (regs->r8) {
		/* Restart the system call - no handlers present */
		if (retval == -ERESTARTNOHAND
		    || retval == -ERESTARTSYS
		    || retval == -ERESTARTNOINTR)
		{
			regs->ea -= 4; /* Size of scall insn.  */
		}
		else if (retval == -ERESTART_RESTARTBLOCK) {
			printk("555 restarting scall\n"); /* todo remove after testing */
			regs->ea -= 4; /* Size of scall insn.  */
		}
	}

	if( handled )
		*handled = 0;

	return retval;
}

asmlinkage int manage_signals(int retval, struct pt_regs* regs) {
	unsigned long flags;

	/* disable interrupts for sampling current_thread_info()->flags */
	local_irq_save(flags);
	while( current_thread_info()->flags & (_TIF_NEED_RESCHED | _TIF_SIGPENDING) ) {
		if( current_thread_info()->flags & _TIF_NEED_RESCHED ) {
			/* schedule -> enables interrupts */
			schedule();

			/* disable interrupts for sampling current_thread_info()->flags */
			local_irq_disable();
		}

		if( current_thread_info()->flags & _TIF_SIGPENDING ) {
#if DEBUG_SIG
			/* debugging code */
			{
				register unsigned long sp asm("sp");
				printk("WILL process signal for %s with regs=%lx, ea=%lx, ba=%lx ra=%lx\n",
						current->comm, regs, regs->ea, regs->ba, *((unsigned long*)(sp+4)));
			}
#endif
			retval = do_signal(retval, regs, NULL);

			/* signal handling enables interrupts */

			/* disable irqs for sampling current_thread_info()->flags */
			local_irq_disable();
#if DEBUG_SIG
			/* debugging code */
			{
				register unsigned long sp asm("sp");
				printk("Processed Signal for %s with regs=%lx, ea=%lx, ba=%lx ra=%lx\n",
						current->comm, regs, regs->ea, regs->ba, *((unsigned long*)(sp+4)));
			}
#endif
		}
	}
	local_irq_restore(flags);

	return retval;
}

asmlinkage void manage_signals_irq(struct pt_regs* regs) {
	unsigned long flags;
	/* do not handle in atomic mode */
	if (unlikely(in_atomic_preempt_off()) && unlikely(!current->exit_state))
		return;

	/* disable interrupts for sampling current_thread_info()->flags */
	local_irq_save(flags);

	if( current_thread_info()->flags & _TIF_NEED_RESCHED ) {
		/* schedule -> enables interrupts */
		schedule();
		
		/* disable interrupts for sampling current_thread_info()->flags */
		local_irq_disable();
	}

	local_irq_restore(flags);
}
