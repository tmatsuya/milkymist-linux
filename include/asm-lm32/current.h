/*
 * Based on:
 * include/asm-blackfin/current.h
 *	(C) Copyright 2000, Lineo, David McCullough <davidm@uclinux.org>
 *
 *	rather than dedicate a register (as the m68k source does), we
 *	just keep a global,  we should probably just change it all to be
 *	current and lose _current_task.
 */

#ifndef _LM32_ASM_CURRENT_H
#define _LM32_ASM_CURRENT_H

#include <linux/thread_info.h>

struct task_struct;

static inline struct task_struct *get_current(void)
{
	return(current_thread_info()->task);
}

#define	current	(get_current())

#endif /* _LM32_ASM_CURRENT_H */
