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
 * include/asm-m68knommu/processor.h
 *
 * Copyright (C) 1995 Hamish Macdonald
 */

#ifndef _ASM_LM32_PROCESSOR_H
#define _ASM_LM32_PROCESSOR_H

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#include <linux/compiler.h>
#include <linux/threads.h>
#include <asm/types.h>
#include <asm/ptrace.h>
#include <asm/current.h>

/*
 * User space process size: 3.75GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */
#define TASK_SIZE	(0xF0000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's. We won't be using it
 */
#define TASK_UNMAPPED_BASE	0

/* 
 * if you change this structure, you must change the code and offsets
 * in asm-offsets.c
 */
   
struct thread_struct {
	unsigned long ksp;	/* kernel stack pointer */
	unsigned long usp;	/* user stack pointer */
	unsigned long which_stack; /* 0 if we are on kernel stack, 1 if we are on user stack */
	void *        debuggerinfo;
};

#define KSTK_TOS(tsk) ((unsigned long)task_stack_page(tsk) + THREAD_SIZE - 32)
#define task_pt_regs(tsk) ((struct pt_regs *)KSTK_TOS(tsk) - 1)
#define KSTK_EIP(tsk) 0
#define KSTK_ESP(tsk) 0

#define INIT_THREAD  { \
	sizeof(init_stack) + (unsigned long) init_stack, 0, \
	0, \
	0 \
}

#define	reformat(_regs)		do { } while (0)

/*
 * Do necessary setup to start up a newly executed thread.
 */
extern void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp);

/* Forward declaration, a strange C thing */
struct task_struct;

/* Free all resources held by a thread. */
static inline void release_thread(struct task_struct *dead_task)
{
}

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)	do { } while (0)

extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/*
 * Free current thread data structures etc..
 */
static inline void exit_thread(void)
{
}

unsigned long thread_saved_pc(struct task_struct *tsk);
unsigned long get_wchan(struct task_struct *p);

#define cpu_relax()    barrier()

#endif
