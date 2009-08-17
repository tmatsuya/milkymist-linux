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

#ifndef _LM32_ASM_THREAD_INFO_H
#define _LM32_ASM_THREAD_INFO_H

#include <asm/page.h>
#include <asm/segment.h>

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

/*
 * Size of kernel stack for each process. This must be a power of 2...
 */
#define THREAD_SIZE_ORDER (2)
#define THREAD_SIZE (PAGE_SIZE<<THREAD_SIZE_ORDER)

/*
 * low level task data.
 */
struct thread_info {
	struct task_struct *task;		/* main task structure */
	struct exec_domain *exec_domain;	/* execution domain */
	unsigned long	   flags;		/* low level flags */
	int		   cpu;			/* cpu we're on */
	int		   preempt_count;	/* 0 => preemptable, <0 => BUG */
	struct restart_block restart_block;
	mm_segment_t	mem_seg; /* USER_DS if user mode thread, KERNEL_DS if kernel mode thread */
};

/*
 * macros/functions for gaining access to the thread information structure
 */
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= 0,			\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
	.mem_seg = KERNEL_DS, \
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)


/*
 * how to get the thread information struct from C
 */
static inline struct thread_info *current_thread_info(void) __attribute_const__;

extern struct thread_info* lm32_current_thread;

static inline struct thread_info *current_thread_info(void)
{
	return lm32_current_thread;
}


/* thread information allocation */
#define alloc_thread_info(tsk) ((struct thread_info *) \
				__get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER))
#define free_thread_info(ti)	free_pages((unsigned long) (ti), THREAD_SIZE_ORDER)
#endif /* __ASSEMBLY__ */

#define	PREEMPT_ACTIVE	0x4000000

/*
 * thread information flag bit numbers
 */
#define TIF_SYSCALL_TRACE	0	/* syscall trace active */
#define TIF_NOTIFY_RESUME	1	/* resumption notification requested */
#define TIF_SIGPENDING		2	/* signal pending */
#define TIF_NEED_RESCHED	3	/* rescheduling necessary */
#define TIF_POLLING_NRFLAG	4	/* true if poll_idle() is polling TIF_NEED_RESCHED */
#define TIF_MEMDIE		5
#define TIF_RESTORE_SIGMASK	6

/* as above, but as bit values */
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)

#define _TIF_WORK_MASK		0x0000FFFE	/* work to do on interrupt/exception return */

#endif /* __KERNEL__ */

#endif /* _LM32_ASM_THREAD_INFO_H */
