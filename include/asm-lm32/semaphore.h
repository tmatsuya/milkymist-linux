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
 * include/asm-v850/semaphore.h
 * include/asm-m68k/semaphore.h
 *
 * Interrupt-safe semaphores..
 *
 * (C) Copyright 1996 Linus Torvalds
 */


#ifndef __ASM_LM32_SEMAPHORE_H
#define __ASM_LM32_SEMAPHORE_H

#ifndef __ASSEMBLY__

#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <asm/atomic.h>

struct semaphore {
	atomic_t count;
	atomic_t waking;
	int sleepers;
	wait_queue_head_t wait;
};

#define __SEMAPHORE_INITIALIZER(name, n)				\
{									\
	.count		= ATOMIC_INIT(n),				\
	.sleepers	= 0,						\
	.wait		= __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
}

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init(struct semaphore *sem, int val)
{
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER(*sem, val);
}

static inline void init_MUTEX(struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED(struct semaphore *sem)
{
	sema_init(sem, 0);
}

asmlinkage void __down(struct semaphore *sem);
asmlinkage int __down_interruptible(struct semaphore *sem);
asmlinkage int __down_trylock(struct semaphore *sem);
asmlinkage void __up(struct semaphore *sem);

extern spinlock_t semaphore_wake_lock;

/*
 * This is ugly, but we want the default case to fall through.
 * "down_failed" is a special asm handler that calls the C
 * routine that actually waits.
 */
static inline void down(struct semaphore *sem)
{
	might_sleep();
	if (atomic_dec_return(&sem->count) < 0)
		__down(sem);
}

static inline int down_interruptible(struct semaphore *sem)
{
	int ret = 0;

	might_sleep();
	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_interruptible(sem);
	return (ret);
}

static inline int down_trylock(struct semaphore *sem)
{
	int ret = 0;

	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_trylock(sem);
	return ret;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
static inline void up(struct semaphore *sem)
{
	if (atomic_inc_return(&sem->count) <= 0)
		__up(sem);
}

#endif				/* __ASSEMBLY__ */
#endif
