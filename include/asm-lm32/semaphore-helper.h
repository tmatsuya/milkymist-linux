/*
 * Based on:
 * include/asm-blackfin/semaphore-helper.h
 * 
 * Based on M68K version,	Lineo Inc.	May 2001
 *
 * SMP- and interrupt-safe semaphores helper functions.
 *
 * (C) Copyright 1996 Linus Torvalds
 *
 */

#ifndef _ASM_LM32_SEMAPHORE_HELPER_H
#define _ASM_LM32_SEMAPHORE_HELPER_H

#include <asm/errno.h>

/*
 * These two _must_ execute atomically wrt each other.
 */
static inline void wake_one_more(struct semaphore *sem)
{
	atomic_inc(&sem->waking);
}

static inline int waking_non_zero(struct semaphore *sem)
{
	int ret;
	unsigned long flags = 0;

	spin_lock_irqsave(&semaphore_wake_lock, flags);
	ret = 0;
	if (atomic_read(&sem->waking) > 0) {
		atomic_dec(&sem->waking);
		ret = 1;
	}
	spin_unlock_irqrestore(&semaphore_wake_lock, flags);
	return ret;
}

/*
 * waking_non_zero_interruptible:
 *	1	got the lock
 *	0	go to sleep
 *	-EINTR	interrupted
 */
static inline int waking_non_zero_interruptible(struct semaphore *sem,
						struct task_struct *tsk)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&semaphore_wake_lock, flags);
	if (atomic_read(&sem->waking) > 0) {
		atomic_dec(&sem->waking);
		ret = 1;
	} else if (signal_pending(tsk)) {
		atomic_inc(&sem->count);
		ret = -EINTR;
	}
	spin_unlock_irqrestore(&semaphore_wake_lock, flags);
	return ret;
}

/*
 * waking_non_zero_trylock:
 *	1	failed to lock
 *	0	got the lock
 */
static inline int waking_non_zero_trylock(struct semaphore *sem)
{
	int ret = 1;
	unsigned long flags = 0;

	spin_lock_irqsave(&semaphore_wake_lock, flags);
	if (atomic_read(&sem->waking) > 0) {
		atomic_dec(&sem->waking);
		ret = 0;
	} else
		atomic_inc(&sem->count);
	spin_unlock_irqrestore(&semaphore_wake_lock, flags);
	return ret;
}

#endif
