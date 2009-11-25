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

#ifndef _LM32_ASM_ATOMIC_H
#define _LM32_ASM_ATOMIC_H

#include <linux/compiler.h> /* likely / unlikely */
#include <asm/system.h>

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * We do not have SMP lm32 systems.
 */

typedef struct { int counter; } atomic_t;
#define ATOMIC_INIT(i)	{ (i) }

#define atomic_read(v)		((v)->counter)
#define atomic_set(v, i)	(((v)->counter) = i)

static __inline__ void atomic_add(int i, atomic_t *v)
{
	unsigned long flags;
	local_irq_save(flags);
	v->counter += i;
	local_irq_restore(flags);
}

static __inline__ void atomic_sub(int i, atomic_t *v)
{
	unsigned long flags;
	local_irq_save(flags);
	v->counter -= i;
	local_irq_restore(flags);
}

static __inline__ int atomic_sub_and_test(int i, atomic_t * v)
{
	int ret;
	unsigned long flags;
	local_irq_save(flags);
	v->counter -= i;
	ret = v->counter != 0;
	local_irq_restore(flags);
	return ret;
}

static __inline__ void atomic_inc(volatile atomic_t *v)
{
	unsigned long flags;
	local_irq_save(flags);
	v->counter++;
	local_irq_restore(flags);
}

/*
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */

static __inline__ int atomic_inc_and_test(volatile atomic_t *v)
{
	int ret;
	unsigned long flags;
	local_irq_save(flags);
	v->counter++;
	ret = v->counter == 0;
	local_irq_restore(flags);
	return ret;
}

static __inline__ void atomic_dec(volatile atomic_t *v)
{
	unsigned long flags;
	local_irq_save(flags);
	v->counter--;
	local_irq_restore(flags);
}

static __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
	int ret;
	unsigned long flags;
	local_irq_save(flags);
	v->counter--;
	ret = v->counter == 0;
	local_irq_restore(flags);
	return ret;
}

static __inline__ void atomic_clear_mask(unsigned long mask, unsigned long *v)
{
	unsigned long flags;
	local_irq_save(flags);
	*v &= mask;
	local_irq_restore(flags);
}

static __inline__ void atomic_set_mask(unsigned long mask, unsigned long *v)
{
	unsigned long flags;
	local_irq_save(flags);
	*v |= mask;
	local_irq_restore(flags);
}

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()    barrier()
#define smp_mb__after_atomic_dec() barrier()
#define smp_mb__before_atomic_inc()    barrier()
#define smp_mb__after_atomic_inc() barrier()

static inline int atomic_add_return(int i, atomic_t * v)
{
	unsigned long temp, flags;

	local_irq_save(flags);
	temp = *(long *)v;
	temp += i;
	*(long *)v = temp;
	local_irq_restore(flags);

	return temp;
}

#define atomic_add_negative(a, v)	(atomic_add_return((a), (v)) < 0)

static inline int atomic_sub_return(int i, atomic_t * v)
{
	unsigned long temp, flags;

	local_irq_save(flags);
	temp = *(long *)v;
	temp -= i;
	*(long *)v = temp;
	local_irq_restore(flags);

	return temp;
}

#define atomic_cmpxchg(v, o, n) ((int)cmpxchg(&((v)->counter), (o), (n)))
#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

static __inline__ int atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	for (;;) {
		if( unlikely(c == (u)) )
			break;
		old = atomic_cmpxchg((v), c, c + (a));
		if( likely(old == c) )
			break;
		c = old;
	}
	return c != (u);
}

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

#define atomic_dec_return(v) atomic_sub_return(1,(v))
#define atomic_inc_return(v) atomic_add_return(1,(v))

#include <asm-generic/atomic.h>
#endif /* _LM32_ATOMIC_H */
